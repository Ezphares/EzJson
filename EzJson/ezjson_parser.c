#include "ezjson_parser.h"

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef EZJSON_PARSER_STACK_SIZE
#define EZJSON_PARSER_STACK_SIZE 8
#endif // !EZJSON_PARSER_STACK_SIZE

#ifndef EZJSON_PARSER_INLINE_BUFFER
#define EZJSON_PARSER_INLINE_BUFFER 128
#endif // !EZJSON_INLINE_BUFFER

#define CHECKED(stmt)                                                          \
    if ((stmt) != 0)                                                           \
    {                                                                          \
        return -1;                                                             \
    }

enum EzJSONParserState
{
    EZ_PS_ERROR = 0,

    EZ_PS_EXPECT_VALUE   = (1 << 0),
    EZ_PS_EXPECT_SEQ_SEP = (1 << 1),
    EZ_PS_EXPECT_KV_SEP  = (1 << 2),
    EZ_PS_EXPECT_OBJ_END = (1 << 3),
    EZ_PS_EXPECT_ARR_END = (1 << 4),
    EZ_PS_EXPECT_OBJ_KEY = (1 << 5),
    EZ_PS_EXPECT_EOF     = (1 << 6),

};

struct EzJSONParser
{
    struct EzJSONParserSettings settings;

    char inlineBuffer[EZJSON_PARSER_STACK_SIZE];
    unsigned bufferSize;
    char *buffer;
    unsigned bufferPos;

    char inlineStack[EZJSON_PARSER_STACK_SIZE];
    unsigned stackSize;
    char *stack;
    unsigned stackTop;

    enum EzJSONParserState state;

    struct EzJSONToken token;

    char peeked;

    char hasPeeked;
    char hasToken;

    unsigned line;
    unsigned pos;
};

// Internal
void setTokenSimple(struct EzJSONParser *parser, enum EzJSONTokenType type)
{
    parser->hasToken   = 1;
    parser->token.type = type;
}

void setTokenNumber(
    struct EzJSONParser *parser, enum EzJSONTokenType type, EzJSONNumber data)
{
    parser->hasToken          = 1;
    parser->token.type        = type;
    parser->token.data_number = data;
}

void setTokenBool(
    struct EzJSONParser *parser, enum EzJSONTokenType type, EzJSONBool data)
{
    parser->hasToken        = 1;
    parser->token.type      = type;
    parser->token.data_bool = data;
}

void setTokenText(
    struct EzJSONParser *parser,
    enum EzJSONTokenType type,
    char *data,
    unsigned len)
{
    parser->hasToken               = 1;
    parser->token.type             = type;
    parser->token.data_text        = data;
    parser->token.data_text_length = len;
}

void resetValue(struct EzJSONParser *parser)
{
    parser->bufferPos = 0;
}

void freeBuffer(struct EzJSONParser *parser)
{
    if (parser->bufferSize != EZJSON_PARSER_INLINE_BUFFER)
    {
        if (parser->settings.free_memory)
        {
            parser->settings.free_memory(
                parser->settings.userdata, parser->buffer, parser->bufferSize);
        }
        else
        {
            free(parser->buffer);
        }
        parser->buffer     = parser->inlineBuffer;
        parser->bufferSize = EZJSON_PARSER_INLINE_BUFFER;
    }
}

void growBuffer(struct EzJSONParser *parser)
{
    const unsigned newSize = parser->bufferSize * 2u;
    char *tmp;
    if (parser->settings.allocate_memory)
    {
        tmp = parser->settings.allocate_memory(
            parser->settings.userdata, newSize);
    }
    else
    {
        tmp = (char *)malloc(newSize);
    }
    memcpy(tmp, parser->buffer, parser->bufferSize);
    freeBuffer(parser);

    parser->buffer = tmp;
    parser->bufferSize *= newSize;
}

void writeBuffer(struct EzJSONParser *parser, char c)
{
    if (parser->bufferPos >= parser->bufferSize)
        growBuffer(parser);
    parser->buffer[parser->bufferPos++] = c;
}

void freeStack(struct EzJSONParser *parser)
{
    if (parser->stackSize != EZJSON_PARSER_STACK_SIZE)
    {
        if (parser->settings.free_memory)
        {
            parser->settings.free_memory(
                parser->settings.userdata, parser->stack, parser->stackSize);
        }
        else
        {
            free(parser->stack);
        }
        parser->stack     = parser->inlineStack;
        parser->stackSize = EZJSON_PARSER_STACK_SIZE;
    }
}

void growStack(struct EzJSONParser *parser)
{
    const unsigned newSize = parser->stackSize * 2u;
    char *tmp;
    if (parser->settings.allocate_memory)
    {
        tmp = parser->settings.allocate_memory(
            parser->settings.userdata, newSize);
    }
    else
    {
        tmp = (char *)malloc(newSize);
    }
    memcpy(tmp, parser->stack, parser->stackSize);
    freeStack(parser);

    parser->stack = tmp;
    parser->stackSize *= newSize;
}

void stackPush(struct EzJSONParser *parser, EzJSONBool bit)
{
    if (parser->stackTop == parser->stackSize * 8u)
    {
        growStack(parser);
    }

    const unsigned stackByte = parser->stackTop / 8u;
    const unsigned stackBit  = parser->stackTop % 8u;

    if (bit)
    {
        parser->stack[stackByte] |= (1u << stackBit);
    }
    else
    {
        parser->stack[stackByte] &= ~(1u << stackBit);
    }
    parser->stackTop++;
}

int stackPop(struct EzJSONParser *parser)
{
    if (parser->stackTop > 0)
    {
        parser->stackTop--;
        return 0;
    }
    return -1;
}

EzJSONBool stackTop(struct EzJSONParser *parser)
{
    const unsigned stackByte = (parser->stackTop - 1) / 8u;
    const unsigned stackBit  = (parser->stackTop - 1) % 8u;
    return (parser->stack[stackByte] & (1u << stackBit)) > 0;
}

int peek(struct EzJSONParser *parser)
{
    if (parser->hasPeeked)
    {
        return 0;
    }

    parser->hasPeeked = parser->settings.get_next_char(
                            parser->settings.userdata, &parser->peeked)
                        == 0;

    parser->peeked &= 0xff;

    return parser->hasPeeked ? 0 : -1;
}

int read(struct EzJSONParser *parser, char *c)
{
    int result = 0;
    if (parser->hasPeeked)
    {
        *c                = parser->peeked;
        parser->hasPeeked = 0;
    }
    else
    {
        result = parser->settings.get_next_char(parser->settings.userdata, c);
    }

    if (*c == '\n')
    {
        parser->line++;
        parser->pos;
    }

    return result;
}

int skip(struct EzJSONParser *parser, char c)
{
    char tmp;
    if (read(parser, &tmp) != 0)
    {
        return -1;
    }

    if (tmp != c)
    {
        return -1;
    }

    return 0;
}

void skipWhitespace(struct EzJSONParser *parser)
{
    char tmp;
    while (peek(parser) == 0
           && (parser->peeked == ' ' || parser->peeked == '\t'
               || parser->peeked == '\n' || parser->peeked == '\r'))
    {
        read(parser, &tmp);
    }
}

int readNull(struct EzJSONParser *parser)
{
    CHECKED(skip(parser, 'n'));
    CHECKED(skip(parser, 'u'));
    CHECKED(skip(parser, 'l'));
    CHECKED(skip(parser, 'l'));

    return 0;
}

int readBool(struct EzJSONParser *parser, EzJSONBool *out)
{
    CHECKED(peek(parser));

    if (parser->peeked == 'f')
    {
        CHECKED(skip(parser, 'f'));
        CHECKED(skip(parser, 'a'));
        CHECKED(skip(parser, 'l'));
        CHECKED(skip(parser, 's'));
        CHECKED(skip(parser, 'e'));

        *out = 0;
    }
    else
    {
        CHECKED(skip(parser, 't'));
        CHECKED(skip(parser, 'r'));
        CHECKED(skip(parser, 'u'));
        CHECKED(skip(parser, 'e'));

        *out = 1;
    }

    return 0;
}

int readNumber(struct EzJSONParser *parser, EzJSONNumber *out)
{
    resetValue(parser);
    CHECKED(peek(parser));

    // Sign
    if (parser->peeked == '-')
    {
        writeBuffer(parser, '-');
        skip(parser, '-');
        CHECKED(peek(parser));
    }

    // Integer part
    if (parser->peeked == '0')
    {
        writeBuffer(parser, '0');
        skip(parser, '0');
        CHECKED(peek(parser));
    }
    else if (parser->peeked >= '1' && parser->peeked <= '9')
    {
        while (parser->peeked >= '0' && parser->peeked <= '9')
        {
            writeBuffer(parser, parser->peeked);
            skip(parser, parser->peeked);
            CHECKED(peek(parser));
        }
    }
    else
    {
        // Integer part is not optional
        return -1;
    }

    // Fraction part
    if (parser->peeked == '.')
    {
        writeBuffer(parser, '.');
        skip(parser, '.');
        CHECKED(peek(parser));

        if (parser->peeked < '0' || parser->peeked > '9')
        {
            // If decimal point is present, at least one decimal digit
            return -1;
        }

        while (parser->peeked >= '0' && parser->peeked <= '9')
        {
            writeBuffer(parser, (char)parser->peeked);
            skip(parser, (char)parser->peeked);
            CHECKED(peek(parser));
        }
    }

    // Exponent part
    if (parser->peeked == 'e' || parser->peeked == 'E')
    {
        writeBuffer(parser, 'e');
        skip(parser, (char)parser->peeked);
        CHECKED(peek(parser));

        if (parser->peeked == '+' || parser->peeked == '-')
        {
            writeBuffer(parser, (char)parser->peeked);
            skip(parser, (char)parser->peeked);
            CHECKED(peek(parser));
        }

        if (parser->peeked < '0' || parser->peeked > '9')
        {
            // If exponent E is present, at least one exponent digit
            return -1;
        }

        while (parser->peeked >= '0' && parser->peeked <= '9')
        {
            writeBuffer(parser, (char)parser->peeked);
            skip(parser, (char)parser->peeked);
            CHECKED(peek(parser));
        }
    }

    writeBuffer(parser, '\x00');
    sscanf_s(parser->buffer, "%f", out);

    return 0;
}

int readString(struct EzJSONParser *parser)
{
    resetValue(parser);
    CHECKED(skip(parser, '"'));

    int done = 0;
    int verb = 0;

    while (peek(parser) == 0 && done == 0)
    {
        if (verb)
        {
            switch ((char)parser->peeked)
            {
            case '\\':
                writeBuffer(parser, '\\');
                break;
            case '"':
                writeBuffer(parser, '"');
                break;
            case 'n':
                writeBuffer(parser, '\n');
                break;
            case 'r':
                writeBuffer(parser, '\r');
                break;
            case 'b':
                writeBuffer(parser, '\b');
                break;
            case 'f':
                writeBuffer(parser, '\f');
                break;
            case 't':
                writeBuffer(parser, '\t');
                break;
            default:
                return -1;
            }
            verb = 0;
        }
        else
        {
            switch ((char)parser->peeked)
            {
            case '"':
                done = 1;
                break;
            case '\\':
                verb = 1;
                break;

            default:
                writeBuffer(parser, (char)parser->peeked);
            }
        }

        if (done == 0)
            parser->hasPeeked = 0;
    }

    CHECKED(skip(parser, '"'));
    return 0;
}

int readValue(struct EzJSONParser *parser)
{
    if (peek(parser) != 0)
    {
        return -1;
    }

    if (parser->peeked == '{')
    {
        skip(parser, '{');
        setTokenSimple(parser, EZJ_TOKEN_OBJ_BEGIN);
        return 0;
    }

    if (parser->peeked == '[')
    {
        skip(parser, '[');
        setTokenSimple(parser, EZJ_TOKEN_ARR_BEGIN);
        return 0;
    }

    if (parser->peeked == 'n')
    {
        if (readNull(parser) != 0)
        {
            return -1;
        }

        setTokenSimple(parser, EZJ_TOKEN_NULL);
        return 0;
    }

    if (parser->peeked == 't' || parser->peeked == 'f')
    {
        EzJSONBool val;
        if (readBool(parser, &val) != 0)
        {
            return -1;
        }

        setTokenBool(parser, EZJ_TOKEN_BOOL, val);
        return 0;
    }

    if (parser->peeked == '-'
        || (parser->peeked >= '0' && parser->peeked <= '9'))
    {
        EzJSONNumber val;
        if (readNumber(parser, &val) != 0)
        {
            return -1;
        }

        setTokenNumber(parser, EZJ_TOKEN_NUMBER, val);
        return 0;
    }

    if (parser->peeked == '"')
    {
        if (readString(parser) != 0)
        {
            return -1;
        }

        setTokenText(
            parser, EZJ_TOKEN_STRING, parser->buffer, parser->bufferPos);
        return 0;
    }

    return -1;
}

// Interface

struct EzJSONParser *EzJSONParserCreate(struct EzJSONParserSettings *settings)
{
    struct EzJSONParser *parser;
    if (settings->allocate_memory)
    {
        parser = (struct EzJSONParser *)settings->allocate_memory(
            settings->userdata, sizeof(struct EzJSONParser));
    }
    else
    {
        parser = malloc(sizeof(struct EzJSONParser));
    }
    memcpy(&parser->settings, settings, sizeof(struct EzJSONParserSettings));

    parser->buffer     = parser->inlineBuffer;
    parser->bufferSize = EZJSON_PARSER_INLINE_BUFFER;
    parser->bufferPos  = 0;
    parser->stack      = parser->inlineStack;
    parser->stackSize  = EZJSON_PARSER_STACK_SIZE;
    parser->stackTop   = 0;
    parser->peeked     = '\0';
    parser->hasPeeked  = 0;
    parser->hasToken   = 0;
    parser->state      = EZ_PS_EXPECT_VALUE;
    parser->line       = 1;
    parser->pos        = 0;
    return parser;
}

struct EzJSONToken *EzJSONParserToken(struct EzJSONParser *parser)
{
    return parser->hasToken ? &parser->token : NULL;
}

enum EzJSONParserState expectedAfterValue(struct EzJSONParser *parser)
{
    if (parser->stackTop == 0)
    {
        return EZ_PS_EXPECT_EOF;
    }
    else
    {
        if (stackTop(parser) == 0)
        {
            return EZ_PS_EXPECT_ARR_END | EZ_PS_EXPECT_SEQ_SEP;
        }
        else
        {
            return EZ_PS_EXPECT_OBJ_END | EZ_PS_EXPECT_SEQ_SEP;
        }
    }
}

void nextToken(struct EzJSONParser *parser)
{
    if (peek(parser) == 0)
    {
        if (parser->state & EZ_PS_EXPECT_ARR_END)
        {
            if (parser->peeked == ']')
            {
                skip(parser, ']');
                stackPop(parser);
                setTokenSimple(parser, EZJ_TOKEN_ARR_END);
                parser->state = expectedAfterValue(parser);
                return;
            }
        }
        if (parser->state & EZ_PS_EXPECT_OBJ_END)
        {
            if (parser->peeked == '}')
            {
                skip(parser, '}');
                stackPop(parser);
                setTokenSimple(parser, EZJ_TOKEN_OBJ_END);
                parser->state = expectedAfterValue(parser);
                return;
            }
        }
        if (parser->state & EZ_PS_EXPECT_SEQ_SEP)
        {
            if (parser->peeked == ',')
            {
                skip(parser, ',');
                setTokenSimple(parser, EZJ_TOKEN_SEQ_SEP);
                parser->state = (stackTop(parser) == 0) ? EZ_PS_EXPECT_VALUE
                                                        : EZ_PS_EXPECT_OBJ_KEY;
                return;
            }
        }
        if (parser->state & EZ_PS_EXPECT_KV_SEP)
        {
            if (parser->peeked == ':')
            {
                skip(parser, ':');
                setTokenSimple(parser, EZJ_TOKEN_KV_SEP);
                parser->state = EZ_PS_EXPECT_VALUE;
                return;
            }
        }
        if (parser->state & EZ_PS_EXPECT_OBJ_KEY)
        {
            if (readString(parser) == 0)
            {
                setTokenText(
                    parser,
                    EZJ_TOKEN_OBJ_KEY,
                    parser->buffer,
                    parser->bufferPos);
                parser->state = EZ_PS_EXPECT_KV_SEP;
                return;
            }
        }
        if (parser->state & EZ_PS_EXPECT_VALUE)
        {
            if (readValue(parser) == 0)
            {
                switch (parser->token.type)
                {
                case EZJ_TOKEN_ARR_BEGIN:
                    stackPush(parser, 0);
                    parser->state = EZ_PS_EXPECT_VALUE;
                    return;
                case EZJ_TOKEN_OBJ_BEGIN:
                    stackPush(parser, 1);
                    parser->state = EZ_PS_EXPECT_OBJ_KEY | EZ_PS_EXPECT_OBJ_END;
                    return;
                default:
                    parser->state = expectedAfterValue(parser);
                    return;
                }
            }
        }
    }
    else if (parser->state & EZ_PS_EXPECT_EOF)
    {
        return;
    }
}

void EzJSONParserNext(struct EzJSONParser *parser)
{
    parser->hasToken = 0;
    skipWhitespace(parser);
    nextToken(parser);

    if (!parser->hasToken && !(parser->state & EZ_PS_EXPECT_EOF))
    {
        parser->state = EZ_PS_ERROR;
    }
}

void EzJSONParserDestroy(struct EzJSONParser *parser)
{
    freeBuffer(parser);
    freeStack(parser);

    if (parser->settings.free_memory)
    {
        parser->settings.free_memory(
            parser->settings.userdata, parser, sizeof(struct EzJSONParser));
    }
    else
    {
        free(parser);
    }
}

EzJSONBool EzJSONParserHasError(struct EzJSONParser *parser)
{
    return parser->state & EZ_PS_ERROR;
}

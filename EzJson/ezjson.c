#include "ezjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define kInitialBufferSize 128

const short kPeekNone = 0x100;

#define CHECKED(stmt)                                                          \
    if ((stmt) != 0)                                                           \
    {                                                                          \
        if (state->reader->onError)                                            \
            state->reader->onError(state->reader->userdata);                   \
        return -1;                                                             \
    }

#define INVOKE_SIMPLE(callback)                                                \
    if (state->reader->callback)                                               \
    {                                                                          \
        state->reader->callback(state->reader->userdata);                      \
    }

#define INVOKE_STRING(callback)                                                \
    if (state->reader->callback)                                               \
    {                                                                          \
        state->reader->callback(                                               \
            state->reader->userdata,                                           \
            state->valueBuffer,                                                \
            state->valueBufferPosition);                                       \
    }

#define INVOKE_BOOL(callback, val)                                             \
    if (state->reader->callback)                                               \
    {                                                                          \
        state->reader->callback(state->reader->userdata, val);                 \
    }

#define INVOKE_NUMBER(callback, val)                                           \
    if (state->reader->callback)                                               \
    {                                                                          \
        state->reader->callback(state->reader->userdata, val);                 \
    }

struct EzJSONParseState
{
    char *valueBuffer;
    unsigned valueBufferSize;
    unsigned valueBufferPosition;

    struct EzJSONReader *reader;
    EzJSONGetChar getChar;
    short peeked;
};

void freeBuffer(struct EzJSONParseState *state)
{
    if (state->valueBufferSize != kInitialBufferSize)
        free(state->valueBuffer);
}

void grow(struct EzJSONParseState *state)
{
    char *tmp = malloc(state->valueBufferSize * 2u);
    memcpy(tmp, state->valueBuffer, state->valueBufferSize);
    freeBuffer(state);

    state->valueBuffer = tmp;
    state->valueBufferSize *= 2u;
}

void resetValue(struct EzJSONParseState *state)
{
    state->valueBufferPosition = 0;
}

void pushValue(struct EzJSONParseState *state, char chr)
{
    if (state->valueBufferPosition >= state->valueBufferSize)
        grow(state);
    state->valueBuffer[state->valueBufferPosition++] = chr;
}

int peek(struct EzJSONParseState *state)
{
    if (state->peeked != kPeekNone)
    {
        return 0;
    }

    char tmp;
    if (state->getChar(state->reader->userdata, &tmp) != 0)
    {
        return -1;
    }

    state->peeked = (short)(tmp & 0xff);
    return 0;
}

int read(struct EzJSONParseState *state, char *chr)
{
    if (state->peeked != kPeekNone)
    {
        *chr          = (char)state->peeked;
        state->peeked = kPeekNone;
        return 0;
    }

    return state->getChar(state->reader->userdata, chr);
}

int skip(struct EzJSONParseState *state, char chr)
{
    char tmp;
    if (read(state, &tmp) != 0)
    {
        // Expected chr, got nothing
        return -1;
    }

    if (tmp != chr)
    {
        // Expected chr, got tmp
        return -1;
    }

    return 0;
}

void skipWhitespace(struct EzJSONParseState *state)
{
    char tmp;
    while (peek(state) == 0 && state->peeked == ' ' || state->peeked == '\t'
           || state->peeked == '\n' || state->peeked == '\r')
        read(state, &tmp);
}

int EzJSONParseValue(struct EzJSONParseState *state);

int EzJSONParseObject(struct EzJSONParseState *state);
int EzJSONParseArray(struct EzJSONParseState *state);

int EzJSONParseNumber(struct EzJSONParseState *state);
int EzJSONParseString(struct EzJSONParseState *state);
int EzJSONParseBool(struct EzJSONParseState *state);
int EzJSONParseNull(struct EzJSONParseState *state);

int EzJSONParse(struct EzJSONReader *reader, EzJSONGetChar getChar)
{
    char stackbuffer[kInitialBufferSize];

    struct EzJSONParseState state;
    state.valueBuffer     = stackbuffer;
    state.valueBufferSize = kInitialBufferSize;
    state.peeked          = kPeekNone;

    state.reader  = reader;
    state.getChar = getChar;

    int result = EzJSONParseValue(&state);
    freeBuffer(&state);

    return result;
}

int EzJSONParseValue(struct EzJSONParseState *state)
{
    skipWhitespace(state);
    CHECKED(peek(state));

    if (state->peeked == '{')
    {
        return EzJSONParseObject(state);
    }

    if (state->peeked == '[')
    {
        return EzJSONParseArray(state);
    }

    if (state->peeked == 'n')
    {
        return EzJSONParseNull(state);
    }

    if (state->peeked == 't' || state->peeked == 'f')
    {
        return EzJSONParseBool(state);
    }

    if (state->peeked == '-' || (state->peeked >= '0' && state->peeked <= '9'))
    {
        return EzJSONParseNumber(state);
    }

    if (state->peeked == '"')
    {
        // String needs to be called back here, since the parse string is used
        // for both keys and string vals
        if (EzJSONParseString(state) == 0)
        {
            INVOKE_STRING(onString);
        }
    }

    return -1;
}

int EzJSONParseObject(struct EzJSONParseState *state)
{
    CHECKED(skip(state, '{'));
    INVOKE_SIMPLE(onObjectBegin);

    skipWhitespace(state);
    CHECKED(peek(state));

    if (state->peeked != '}')
    {
        while (1)
        {
            CHECKED(EzJSONParseString(state));

            INVOKE_STRING(onKey);

            skipWhitespace(state);
            CHECKED(skip(state, ':'));
            skipWhitespace(state);
            CHECKED(EzJSONParseValue(state));
            skipWhitespace(state);

            CHECKED(peek(state));

            if (state->peeked != ',')
            {
                break;
            }

            skip(state, ',');
            skipWhitespace(state);
        }
    }

    CHECKED(skip(state, '}'));
    INVOKE_SIMPLE(onObjectEnd);
    return 0;
}

int EzJSONParseArray(struct EzJSONParseState *state)
{
    CHECKED(skip(state, '['));
    INVOKE_SIMPLE(onArrayBegin);

    skipWhitespace(state);
    CHECKED(peek(state));

    if (state->peeked != ']')
    {
        while (1)
        {
            CHECKED(EzJSONParseValue(state));
            skipWhitespace(state);
            CHECKED(peek(state));

            if (state->peeked != ',')
            {
                break;
            }

            skip(state, ',');
            skipWhitespace(state);
        }
    }

    CHECKED(skip(state, ']'));
    INVOKE_SIMPLE(onArrayEnd);
    return 0;
}

int EzJSONParseString(struct EzJSONParseState *state)
{
    resetValue(state);
    CHECKED(skip(state, '"'));

    int done = 0;
    int verb = 0;

    while (peek(state) == 0 && done == 0)
    {
        if (verb)
        {
            switch ((char)state->peeked)
            {
            case '\\':
                pushValue(state, '\\');
                break;
            case '"':
                pushValue(state, '"');
                break;
            case 'n':
                pushValue(state, '\n');
                break;
            case 'r':
                pushValue(state, '\r');
                break;
            case 'b':
                pushValue(state, '\b');
                break;
            case 'f':
                pushValue(state, '\f');
                break;
            case 't':
                pushValue(state, '\t');
                break;
            default:
                return -1;
            }
            verb = 0;
        }
        else
        {
            switch ((char)state->peeked)
            {
            case '"':
                done = 1;
                break;
            case '\\':
                verb = 1;
                break;

            default:
                pushValue(state, (char)state->peeked);
            }
        }

        if (done == 0)
            state->peeked = kPeekNone;
    }

    CHECKED(skip(state, '"'));
    return 0;
}

int EzJSONParseBool(struct EzJSONParseState *state)
{
    CHECKED(peek(state));

    if (state->peeked == 'f')
    {
        CHECKED(skip(state, 'f'));
        CHECKED(skip(state, 'a'));
        CHECKED(skip(state, 'l'));
        CHECKED(skip(state, 's'));
        CHECKED(skip(state, 'e'));
        INVOKE_BOOL(onBool, 0);
    }
    else
    {
        CHECKED(skip(state, 't'));
        CHECKED(skip(state, 'r'));
        CHECKED(skip(state, 'u'));
        CHECKED(skip(state, 'e'));
        INVOKE_BOOL(onBool, 1);
    }

    return 0;
}

int EzJSONParseNull(struct EzJSONParseState *state)
{
    CHECKED(skip(state, 'n'));
    CHECKED(skip(state, 'u'));
    CHECKED(skip(state, 'l'));
    CHECKED(skip(state, 'l'));

    INVOKE_SIMPLE(onNull);

    return 0;
}

int EzJSONParseNumber(struct EzJSONParseState *state)
{
    resetValue(state);
    CHECKED(peek(state));

    // Sign
    if (state->peeked == '-')
    {
        pushValue(state, '-');
        skip(state, '-');
        CHECKED(peek(state));
    }

    // Integer part
    if (state->peeked == '0')
    {
        pushValue(state, '0');
        skip(state, '0');
        CHECKED(peek(state));
    }
    else if (state->peeked >= '1' && state->peeked <= '9')
    {
        while (state->peeked >= '0' && state->peeked <= '9')
        {
            pushValue(state, (char)state->peeked);
            skip(state, (char)state->peeked);
            CHECKED(peek(state));
        }
    }
    else
    {
        // Integer part is not optional
        INVOKE_SIMPLE(onError);
        return -1;
    }

    // Fraction part
    if (state->peeked == '.')
    {
        pushValue(state, '.');
        skip(state, '.');
        CHECKED(peek(state));

        if (state->peeked < '0' || state->peeked > '9')
        {
            // If decimal point is present, at least one decimal digit
            INVOKE_SIMPLE(onError);
            return -1;
        }

        while (state->peeked >= '0' && state->peeked <= '9')
        {
            pushValue(state, (char)state->peeked);
            skip(state, (char)state->peeked);
            CHECKED(peek(state));
        }
    }

    // Exponent part
    if (state->peeked == 'e' || state->peeked == 'E')
    {
        pushValue(state, 'e');
        skip(state, (char)state->peeked);
        CHECKED(peek(state));

        if (state->peeked == '+' || state->peeked == '-')
        {
            pushValue(state, (char)state->peeked);
            skip(state, (char)state->peeked);
            CHECKED(peek(state));
        }

        if (state->peeked < '0' || state->peeked > '9')
        {
            // If exponent E is present, at least one exponent digit
            INVOKE_SIMPLE(onError);
            return -1;
        }

        while (state->peeked >= '0' && state->peeked <= '9')
        {
            pushValue(state, (char)state->peeked);
            skip(state, (char)state->peeked);
            CHECKED(peek(state));
        }
    }

    pushValue(state, '\x00');
    EzJSONNumber number = 0.0f;
    sscanf_s(state->valueBuffer, "%f", &number);
    INVOKE_NUMBER(onNumber, number);

    return 0;
}
//////////////////////////////////////////////////////////////////////////

#define WS_COMMA 1
#define WS_CLOSE 2

void flushBuffer(struct EzJSONWriter *writer)
{
    writer->writeBuffer(
        writer->userdata, writer->state.buffer, writer->state.bufferPos);
    writer->state.bufferPos = 0u;
}

void writeData(struct EzJSONWriter *writer, const char *data, unsigned count)
{
    unsigned bufLeft = EZJSON_WRITE_BUFFER_SIZE - writer->state.bufferPos;

    while (count >= bufLeft)
    {
        memcpy(writer->state.buffer + writer->state.bufferPos, data, bufLeft);
        writer->state.bufferPos = EZJSON_WRITE_BUFFER_SIZE;
        data += bufLeft;
        count -= bufLeft;
        flushBuffer(writer);
        writer->state.bufferPos = 0;
        bufLeft                 = EZJSON_WRITE_BUFFER_SIZE;
    }

    if (count > 0)
    {
        memcpy(writer->state.buffer + writer->state.bufferPos, data, count);
        writer->state.bufferPos += count;
    }
}

#if defined(EZJSON_CHECKED_WRITE)
void stackId(int *idx, int *bit, int position)
{
    *idx = position / 8;
    *bit = position % 8;
}

void pushArray(struct EzJSONWriter *writer)
{
    int idx, bit;
    stackId(&idx, &bit, writer->state.stackpos++);
    writer->state.stack[idx] &= ~(1 << bit);
}

void pushObject(struct EzJSONWriter *writer)
{
    int idx, bit;
    stackId(&idx, &bit, writer->state.stackpos++);
    writer->state.stack[idx] |= (1 << bit);
}

int popArray(struct EzJSONWriter *writer)
{
    if (writer->state.stackpos == 0)
    {
        // ERROR
        return -1;
    }

    int idx, bit;
    stackId(&idx, &bit, --writer->state.stackpos);
    if (idx < 4 && (writer->state.stack[idx] & (1 << bit)) != 0)
    {
        // ERROR
        return -1;
    }

    return 0;
}

int popObject(struct EzJSONWriter *writer)
{
    if (writer->state.stackpos == 0)
    {
        // ERROR
        return -1;
    }

    int idx, bit;
    stackId(&idx, &bit, --writer->state.stackpos);
    if (idx < 4 && (writer->state.stack[idx] & (1 << bit)) == 0)
    {
        // ERROR
        return -1;
    }

    return 0;
}

int top(struct EzJSONWriter *writer)
{
    if (writer->state.stackpos == 0)
    {
        // ERROR
        return -1;
    }

    int idx, bit;
    stackId(&idx, &bit, writer->state.stackpos - 1);

    return ((writer->state.stack[idx] & (1 << bit)) > 0) ? 1 : 0;
}

void onError(struct EzJSONWriter *writer, enum EzJSONWriteError errorCode)
{
    writer->error = errorCode;
    if (writer->writeError)
    {
        writer->writeError(writer);
    }
}
#endif

#if defined(EZJSON_PRETTY)
void indent(struct EzJSONWriter *writer)
{
    writer->state.prettyLevel++;
}

void dedent(struct EzJSONWriter *writer)
{
    if (writer->state.prettyLevel > 0)
    {
        writer->state.prettyLevel--;
    }
}

void newLine(struct EzJSONWriter *writer)
{
    writeData(writer, "\n", 1u);
    for (int i = 0; i < writer->state.prettyLevel * writer->state.indentCount;
         ++i)
    {
        writeData(writer, &writer->state.indentChar, 1u);
    }
}
#endif

void newValue(struct EzJSONWriter *writer)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (writer->state.stackpos > 0)
    {
        if (top(writer) == 1 && (writer->state.writestate & WS_CLOSE) > 0)
        {
            onError(writer, EZ_WE_KEY_EXPECTED);
            return;
        }
    }
#endif

    if ((writer->state.writestate & WS_COMMA) > 0)
    {
        writeData(writer, ",", 1u);
#if defined(EZJSON_PRETTY)
        if (writer->state.prettyEnabled)
        {
            newLine(writer);
        }
#endif
    }
    writer->state.writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONInitWriter(struct EzJSONWriter *writer)
{
    writer->state.writestate = 0;
    writer->state.bufferPos  = 0;
    writer->error            = 0;
    writer->writeError       = 0;
#if defined(EZJSON_CHECKED_WRITE)
    writer->state.stacksize = 4 * 8;
    writer->state.stackpos  = 0;
#endif
#if defined(EZJSON_PRETTY)
    writer->state.indentChar    = ' ';
    writer->state.indentCount   = 2;
    writer->state.prettyLevel   = 0;
    writer->state.prettyEnabled = 0;
#endif
}

void EzJSONEnablePrettyPrinting(struct EzJSONWriter *writer, EzJSONBool enabled)
{
#if defined(EZJSON_PRETTY)
    writer->state.prettyEnabled = enabled;
#endif
}

void EzJSONWriteObjectBegin(struct EzJSONWriter *writer)
{
    newValue(writer);
#if defined(EZJSON_CHECKED_WRITE)
    pushObject(writer);
#endif
    writeData(writer, "{", 1u);
#if defined(EZJSON_PRETTY)
    indent(writer);
    if (writer->state.prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writer->state.writestate = WS_CLOSE;
}

void EzJSONWriteObjectEnd(struct EzJSONWriter *writer)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (popObject(writer) != 0 || (writer->state.writestate & WS_CLOSE) == 0)
    {
        onError(writer, EZ_WE_WAS_ARRAY);
        return;
    }
#endif
#if defined(EZJSON_PRETTY)
    dedent(writer);
    if (writer->state.prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writeData(writer, "}", 1u);
    flushBuffer(writer);
    writer->state.writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONWriteArrayBegin(struct EzJSONWriter *writer)
{
    newValue(writer);
#if defined(EZJSON_CHECKED_WRITE)
    pushArray(writer);
#endif
    writeData(writer, "[", 1u);
#if defined(EZJSON_PRETTY)
    indent(writer);
    if (writer->state.prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writer->state.writestate = WS_CLOSE;
}

void EzJSONWriteArrayEnd(struct EzJSONWriter *writer)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (popArray(writer) != 0)
    {
        onError(writer, EZ_WE_WAS_OBJECT);
        return;
    }
#endif
#if defined(EZJSON_PRETTY)
    dedent(writer);
    if (writer->state.prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writeData(writer, "]", 1u);
    flushBuffer(writer);
    writer->state.writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONWriteKey(
    struct EzJSONWriter *writer, const char *str, unsigned count)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (top(writer) != 1 || (writer->state.writestate & WS_CLOSE) == 0)
    {
        onError(writer, EZ_WE_VALUE_EXPECTED);
        return;
    }
    writer->state.writestate &= ~WS_CLOSE;
#endif
    newValue(writer);
    writer->state.writestate = 0;
    writeData(writer, "\"", 1u);
    writeData(writer, str, count);
    writeData(writer, "\":", 2u);
#if defined(EZJSON_PRETTY)
    if (writer->state.prettyEnabled)
    {
        writeData(writer, " ", 1u);
    }
#endif
}

void EzJSONWriteString(
    struct EzJSONWriter *writer, const char *str, unsigned count)
{
    newValue(writer);
    writeData(writer, "\"", 1u);
    writeData(writer, str, count);
    writeData(writer, "\"", 1u);
}

void EzJSONWriteBool(struct EzJSONWriter *writer, EzJSONBool val)
{
    newValue(writer);
    if (val)
    {
        writeData(writer, "true", 4u);
    }
    else if (val)
    {
        writeData(writer, "false", 5u);
    }
}

void EzJSONWriteNull(struct EzJSONWriter *writer)
{
    newValue(writer);
    writeData(writer, "null", 4u);
}

void EzJSONWriteNumber(struct EzJSONWriter *writer, EzJSONNumber val)
{
    char buffer[64];
    snprintf(buffer, 64, "%g", val);

    newValue(writer);
    writeData(writer, buffer, strlen(buffer));
}

void EzJSONWriteNumberL(struct EzJSONWriter *writer, int val)
{
    char buffer[64];
    snprintf(buffer, 64, "%d", val);

    newValue(writer);
    writeData(writer, buffer, strlen(buffer));
}

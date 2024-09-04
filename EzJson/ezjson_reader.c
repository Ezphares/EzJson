#include "ezjson_reader.h"

#define INVOKE_0(func)                                                         \
    {                                                                          \
        if (reader->func)                                                      \
            reader->func(reader->userdata);                                    \
    }

#define INVOKE_1(func, arg1)                                                   \
    {                                                                          \
        if (reader->func)                                                      \
            reader->func(reader->userdata, arg1);                              \
    }

#define INVOKE_2(func, arg1, arg2)                                             \
    {                                                                          \
        if (reader->func)                                                      \
            reader->func(reader->userdata, arg1, arg2);                        \
    }

void EzJSONRead(struct EzJSONReader *reader, struct EzJSONParser *parser)
{
    struct EzJSONToken *token;
    while (EzJSONParserNext(parser), token = EzJSONParserToken(parser))
    {
        switch (token->type)
        {
        case EZJ_TOKEN_OBJ_BEGIN:
            INVOKE_0(onObjectBegin);
            break;

        case EZJ_TOKEN_OBJ_END:
            INVOKE_0(onObjectEnd);
            break;

        case EZJ_TOKEN_ARR_BEGIN:
            INVOKE_0(onArrayBegin);
            break;

        case EZJ_TOKEN_ARR_END:
            INVOKE_0(onArrayEnd);
            break;

        case EZJ_TOKEN_OBJ_KEY:
            INVOKE_2(onKey, token->data_text, token->data_text_length);
            break;

        case EZJ_TOKEN_NULL:
            INVOKE_0(onNull);
            break;

        case EZJ_TOKEN_BOOL:
            INVOKE_1(onBool, token->data_bool);
            break;

        case EZJ_TOKEN_NUMBER:
            INVOKE_1(onNumber, token->data_number);
            break;

        case EZJ_TOKEN_STRING:
            INVOKE_2(onString, token->data_text, token->data_text_length);
            break;

        default:
            break;
        }
    }
}
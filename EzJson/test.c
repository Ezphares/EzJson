#include "ezjson_parser.h"
#include "ezjson_writer.h"

#include <stdio.h>
#include <string.h>

void objB(void *d)
{
    printf("Object Begin\n");
}

void objE(void *d)
{
    printf("Object End\n");
}

void arrB(void *d)
{
    printf("Array Begin\n");
}

void arrE(void *d)
{
    printf("Array End\n");
}

void key(void *d, const char *k, unsigned l)
{
    printf("k: %.*s  v: ", l, k);
}

void number(void *d, float n)
{
    printf("Number: %f\n", n);
}

void error(void *d)
{
    printf("PARSE ERROR\n");
}

struct JSONTester
{
    char *str;
    unsigned pos;
};

int testGetChar(void *d, char *c)
{
    struct JSONTester *t = (struct JSONTester *)d;

    if (t->pos >= strlen(t->str))
    {
        return -1;
    }

    *c = t->str[t->pos++];
    return 0;
}

void dump(void *f, const char *d, unsigned c)
{
    fprintf((FILE *)f, "%.*s", c, d);
}

void writeError(struct EzJSONWriter *writer)
{
    printf("Error while writing checked data\n");
}

int main()
{
    struct JSONTester test;
    test.pos = 0;
    test.str = "{"
               "  \"test\": {},"
               "  \"test2\": [1, -2.0, 0.1, 1e2, 1e-3]"
               "}";

    struct EzJSONParserSettings settings = {0};

    settings.get_next_char = &testGetChar;
    settings.userdata      = &test;

    struct EzJSONParser *parser = EzJSONParserCreate(&settings);
    EzJSONParserNext(parser);

    struct EzJSONToken *token;
    while (token = EzJSONParserToken(parser))
    {
        switch (token->type)
        {
        case EZJ_TOKEN_OBJ_BEGIN:
            printf("{\n");
            break;
        case EZJ_TOKEN_OBJ_END:
            printf("}\n");
            break;
        case EZJ_TOKEN_ARR_BEGIN:
            printf("[\n");
            break;
        case EZJ_TOKEN_ARR_END:
            printf("]\n");
            break;
        case EZJ_TOKEN_SEQ_SEP:
            printf(",\n");
            break;
        case EZJ_TOKEN_KV_SEP:
            printf(":\n");
            break;
        case EZJ_TOKEN_OBJ_KEY:
            printf("\"%.*s\"\n", token->data_text_length, token->data_text);
            break;
        case EZJ_TOKEN_STRING:
            printf("\"%.*s\"\n", token->data_text_length, token->data_text);
            break;
        case EZJ_TOKEN_NUMBER:
            printf("%f\n", token->data_number);
            break;
        case EZJ_TOKEN_BOOL:
            printf("%d\n", token->data_bool);
            break;
        case EZJ_TOKEN_NULL:
            printf("null\n");
            break;
        }

        EzJSONParserNext(parser);
    }

    EzJSONParserDestroy(parser);

    struct EzJSONWriter writer;
    EzJSONInitWriter(&writer);
    writer.userdata    = stdout;
    writer.writeBuffer = &dump;
    writer.writeError  = &writeError;

    EzJSONEnablePrettyPrinting(&writer, 1);

    EzJSONWriteObjectBegin(&writer);
    EzJSONWriteKey(&writer, "bool", 4);
    EzJSONWriteBool(&writer, 1);
    EzJSONWriteKey(&writer, "null", 4);
    EzJSONWriteNull(&writer);
    EzJSONWriteKey(&writer, "num1", 4);
    EzJSONWriteNumber(&writer, 3.14f);
    EzJSONWriteKey(&writer, "num2", 4);
    EzJSONWriteNumberL(&writer, 12345);
    EzJSONWriteKey(&writer, "array", 5);
    EzJSONWriteArrayBegin(&writer);
    EzJSONWriteNumberL(&writer, 0);
    EzJSONWriteNumber(&writer, 1.0);
    EzJSONWriteString(&writer, "two", 3);
    EzJSONWriteArrayEnd(&writer);
    EzJSONWriteObjectEnd(&writer);

    return 0;
}
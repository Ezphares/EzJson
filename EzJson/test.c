#include "ezjson.h"

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

    struct EzJSONReader reader;
    memset(&reader, 0, sizeof(struct EzJSONReader));
    reader.userdata      = &test;
    reader.onObjectBegin = &objB;
    reader.onObjectEnd   = &objE;
    reader.onArrayBegin  = &arrB;
    reader.onArrayEnd    = &arrE;
    reader.onKey         = &key;
    reader.onNumber      = &number;
    reader.onError       = &error;

    EzJSONParse(&reader, testGetChar);

    printf("\n\n");

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
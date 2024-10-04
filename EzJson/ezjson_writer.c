#include "ezjson_writer.h"
#include "ezjson_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////

#define WS_COMMA 1
#define WS_CLOSE 2

void flushBuffer(struct EzJSONWriter *writer)
{
    writer->writeBuffer(
        writer->settings.userdata, writer->buffer, writer->bufferPos);
    writer->bufferPos = 0u;
}

void writeData(struct EzJSONWriter *writer, const char *data, unsigned count)
{
    unsigned bufLeft = EZJSON_WRITE_BUFFER_SIZE - writer->bufferPos;

    while (count >= bufLeft)
    {
        memcpy(writer->buffer + writer->bufferPos, data, bufLeft);
        writer->bufferPos = EZJSON_WRITE_BUFFER_SIZE;
        data += bufLeft;
        count -= bufLeft;
        flushBuffer(writer);
        writer->bufferPos = 0;
        bufLeft           = EZJSON_WRITE_BUFFER_SIZE;
    }

    if (count > 0)
    {
        memcpy(writer->buffer + writer->bufferPos, data, count);
        writer->bufferPos += count;
    }
}

#if defined(EZJSON_CHECKED_WRITE)
void stackId(int *idx, int *bit, int position)
{
    *idx = position / 8;
    *bit = position % 8;
}

int popArray(struct EzJSONWriter *writer)
{
    if (stack_empty(&writer->stack))
    {
        // ERROR
        return -1;
    }

    if (stack_top(&writer->stack) != STACK_BIT_ARRAY)
    {
        // ERROR
        return -1;
    }

    stack_pop(&writer->stack);

    return 0;
}

int popObject(struct EzJSONWriter *writer)
{
    if (stack_empty(&writer->stack))
    {
        // ERROR
        return -1;
    }

    if (stack_top(&writer->stack) != STACK_BIT_OBJECT)
    {
        // ERROR
        return -1;
    }

    stack_pop(&writer->stack);

    return 0;
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
    writer->prettyLevel++;
}

void dedent(struct EzJSONWriter *writer)
{
    if (writer->prettyLevel > 0)
    {
        writer->prettyLevel--;
    }
}

void newLine(struct EzJSONWriter *writer)
{
    writeData(writer, "\n", 1u);
    for (int i = 0; i < writer->prettyLevel * writer->indentCount; ++i)
    {
        writeData(writer, &writer->indentChar, 1u);
    }
}
#endif

void newValue(struct EzJSONWriter *writer)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (!stack_empty(&writer->stack))
    {
        if (stack_top(&writer->stack) == STACK_BIT_OBJECT
            && (writer->writestate & WS_CLOSE) > 0)
        {
            onError(writer, EZ_WE_KEY_EXPECTED);
            return;
        }
    }
#endif

    if ((writer->writestate & WS_COMMA) > 0)
    {
        writeData(writer, ",", 1u);
#if defined(EZJSON_PRETTY)
        if (writer->prettyEnabled)
        {
            newLine(writer);
        }
#endif
    }
    writer->writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONWriterInit(struct EzJSONWriter *writer)
{
    writer->writestate = 0;
    writer->bufferPos  = 0;
    writer->error      = 0;
    writer->writeError = 0;
#if defined(EZJSON_CHECKED_WRITE)
    stack_init(&writer->stack);
#endif
#if defined(EZJSON_PRETTY)
    writer->indentChar    = ' ';
    writer->indentCount   = 2;
    writer->prettyLevel   = 0;
    writer->prettyEnabled = 0;
#endif
}

void EzJSONWriterDestroy(struct EzJSONWriter *writer)
{
    stack_destroy(
        &writer->stack,
        writer->settings.userdata,
        writer->settings.free_memory);
}

void EzJSONEnablePrettyPrinting(struct EzJSONWriter *writer, EzJSONBool enabled)
{
#if defined(EZJSON_PRETTY)
    writer->prettyEnabled = enabled;
#endif
}

void EzJSONWriteObjectBegin(struct EzJSONWriter *writer)
{
    newValue(writer);
#if defined(EZJSON_CHECKED_WRITE)
    stack_push(
        &writer->stack,
        STACK_BIT_OBJECT,
        writer->settings.userdata,
        writer->settings.allocate_memory,
        writer->settings.free_memory);
#endif
    writeData(writer, "{", 1u);
#if defined(EZJSON_PRETTY)
    indent(writer);
    if (writer->prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writer->writestate = WS_CLOSE;
}

void EzJSONWriteObjectEnd(struct EzJSONWriter *writer)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (popObject(writer) != 0 || (writer->writestate & WS_CLOSE) == 0)
    {
        onError(writer, EZ_WE_WAS_ARRAY);
        return;
    }
#endif
#if defined(EZJSON_PRETTY)
    dedent(writer);
    if (writer->prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writeData(writer, "}", 1u);
    flushBuffer(writer);
    writer->writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONWriteArrayBegin(struct EzJSONWriter *writer)
{
    newValue(writer);
#if defined(EZJSON_CHECKED_WRITE)
    stack_push(
        &writer->stack,
        STACK_BIT_ARRAY,
        writer->settings.userdata,
        writer->settings.allocate_memory,
        writer->settings.free_memory);
#endif
    writeData(writer, "[", 1u);
#if defined(EZJSON_PRETTY)
    indent(writer);
    if (writer->prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writer->writestate = WS_CLOSE;
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
    if (writer->prettyEnabled)
    {
        newLine(writer);
    }
#endif
    writeData(writer, "]", 1u);
    flushBuffer(writer);
    writer->writestate = WS_COMMA | WS_CLOSE;
}

void EzJSONWriteKey(
    struct EzJSONWriter *writer, const char *str, unsigned count)
{
#if defined(EZJSON_CHECKED_WRITE)
    if (stack_empty(&writer->stack)
        || stack_top(&writer->stack) != STACK_BIT_OBJECT
        || (writer->writestate & WS_CLOSE) == 0)
    {
        onError(writer, EZ_WE_VALUE_EXPECTED);
        return;
    }
    writer->writestate &= ~WS_CLOSE;
#endif
    newValue(writer);
    writer->writestate = 0;
    writeData(writer, "\"", 1u);
    writeData(writer, str, count);
    writeData(writer, "\":", 2u);
#if defined(EZJSON_PRETTY)
    if (writer->prettyEnabled)
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

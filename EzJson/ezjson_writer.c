#include "ezjson_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef __EZJSON_H_INCLUDED__
#define __EZJSON_H_INCLUDED__

#ifdef EZJSON_NUMBER
typedef EZJSON_NUMBER EzJSONNumber;
#else
typedef float EzJSONNumber;
#endif // !EZJSON_NUMBER

#ifdef EZJSON_BOOL
typedef EZJSON_BOOL EzJSONBool;
#else
typedef _Bool EzJSONBool;
#endif // !EZJSON_BOOL

#ifndef EZJSON_WRITE_BUFFER_SIZE
#define EZJSON_WRITE_BUFFER_SIZE 1024
#endif

#if defined(EZJSON_CHECKED_WRITE)
#ifndef EZJSON_CHECKED_WRITE_STACK_SIZE
#define EZJSON_CHECKED_WRITE_STACK_SIZE 8
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    struct EzJSONReader
    {
        void (*onObjectBegin)(void *);
        void (*onObjectEnd)(void *);
        void (*onArrayBegin)(void *);
        void (*onArrayEnd)(void *);

        void (*onKey)(void *, const char *, unsigned);

        void (*onNull)(void *);
        void (*onNumber)(void *, EzJSONNumber);
        void (*onBool)(void *, EzJSONBool);
        void (*onString)(void *, const char *, unsigned);
        void (*onError)(void *);

        void *userdata;
    };

    // Char should be set to the next input. Return 0 on success, non-zero on
    // error or EOF
    typedef int (*EzJSONGetChar)(void *, char *);
    int EzJSONParse(struct EzJSONReader *reader, EzJSONGetChar getChar);

    enum EzJSONWriteError
    {
        EZ_WE_OK,             // Ok
        EZ_WE_KEY_EXPECTED,   // Expected key, got value
        EZ_WE_VALUE_EXPECTED, // Expected value, got key
        EZ_WE_WAS_ARRAY,      // Got EndObject while writing array
        EZ_WE_WAS_OBJECT,     // Got EndArray while writing object
    };

    struct EzJSONWriter
    {
        void *userdata;

        void (*writeBuffer)(void *, const char *, unsigned);
        void (*writeError)(struct EzJSONWriter *writer);

        enum EzJSONWriteError error;

        struct
        {
            int writestate;
            char buffer[EZJSON_WRITE_BUFFER_SIZE];
            unsigned bufferPos;
#if defined(EZJSON_CHECKED_WRITE)
            unsigned char stack[EZJSON_CHECKED_WRITE_STACK_SIZE];
            int stacksize;
            int stackpos;
#endif
#if defined(EZJSON_PRETTY)
            char indentChar;
            int indentCount;
            int prettyLevel;
            EzJSONBool prettyEnabled;
#endif
        } state;
    };

    void EzJSONInitWriter(struct EzJSONWriter *writer);

    void
    EzJSONEnablePrettyPrinting(struct EzJSONWriter *writer, EzJSONBool enabled);

    void EzJSONWriteObjectBegin(struct EzJSONWriter *writer);
    void EzJSONWriteObjectEnd(struct EzJSONWriter *writer);
    void EzJSONWriteArrayBegin(struct EzJSONWriter *writer);
    void EzJSONWriteArrayEnd(struct EzJSONWriter *writer);

    void EzJSONWriteKey(
        struct EzJSONWriter *writer, const char *str, unsigned count);
    void EzJSONWriteString(
        struct EzJSONWriter *writer, const char *str, unsigned count);
    void EzJSONWriteBool(struct EzJSONWriter *writer, EzJSONBool val);
    void EzJSONWriteNull(struct EzJSONWriter *writer);
    void EzJSONWriteNumber(struct EzJSONWriter *writer, EzJSONNumber val);
    void EzJSONWriteNumberL(struct EzJSONWriter *writer, int val);

#ifdef __cplusplus
}
#endif

#endif

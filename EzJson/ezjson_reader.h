#ifndef __EZJSON_READER_H_INCLUDED__
#define __EZJSON_READER_H_INCLUDED__

#include "ezjson_parser.h"

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

    void EzJSONRead(struct EzJSONReader *, struct EzJSONParser *);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //!__EZJSON_READER_H_INCLUDED__

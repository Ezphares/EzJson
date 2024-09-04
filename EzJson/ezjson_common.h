#ifndef __EZJSON_UTIL_H_INCLUDED__
#define __EZJSON_UTIL_H_INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#ifdef EZJSON_NUMBER
    typedef EZJSON_NUMBER EzJSONNumber;
#else
typedef float EzJSONNumber;
#endif // !EZJSON_NUMBER

#ifdef EZJSON_BOOL
    typedef EZJSON_BOOL EzJSONBool;
#else
typedef char EzJSONBool;
#endif // !EZJSON_BOOL

    // Char should be set to the next input. Return 0 on success, non-zero on
    // error or EOF
    typedef int (*EzJSONGetChar)(void *, char *);

    // Optional allocator overrides
    typedef char *(*EzJSONAlloc)(void *userdata, unsigned size);
    typedef void (*EzJSONFree)(void *userdata, void *ptr, unsigned size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
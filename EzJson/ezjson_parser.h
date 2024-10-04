#ifndef __EZJSON_PARSER_H_INCLUDED__
#define __EZJSON_PARSER_H_INCLUDED__

#include "ezjson_common.h"

#ifndef EZJSON_PARSER_INLINE_BUFFER
#define EZJSON_PARSER_INLINE_BUFFER 128
#endif // !EZJSON_INLINE_BUFFER

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    // Char should be set to the next input. Return 0 on success, non-zero on
    // error or EOF
    typedef int (*EzJSONGetChar)(void *, char *);

    struct EzJSONParserSettings
    {
        void *userdata;              // Optional, passed to all callbacks
        EzJSONGetChar get_next_char; // Mandatory, retrieve next char, return 0
                                     // on success, non-0 on EOF
        EzJSONAlloc allocate_memory; // Optional, uses malloc() if not set
        EzJSONFree free_memory;      // Optional, uses free() if not set
    };

    enum EzJSONTokenType
    {
        EZJ_TOKEN_OBJ_BEGIN, // {
        EZJ_TOKEN_OBJ_END,   // }
        EZJ_TOKEN_ARR_BEGIN, // [
        EZJ_TOKEN_ARR_END,   // }
        EZJ_TOKEN_SEQ_SEP,   // ,
        EZJ_TOKEN_KV_SEP,    // :
        EZJ_TOKEN_OBJ_KEY,
        EZJ_TOKEN_STRING,
        EZJ_TOKEN_NUMBER,
        EZJ_TOKEN_BOOL,
        EZJ_TOKEN_NULL,
    };

    struct EzJSONToken
    {
        enum EzJSONTokenType type;
        union
        {
            // Used with EZJ_TOKEN_NUMBER
            EzJSONNumber data_number;
            // Used with EZJ_TOKEN_BOOL
            EzJSONBool data_bool;
            // Used with EZJ_TOKEN_STRING and EZJ_TOKEN_OBJ_KEY
            struct
            {
                const char *data_text;
                unsigned data_text_length;
            };
        };
    };

    struct EzJSONParser
    {
        struct EzJSONParserSettings settings;

        char inlineBuffer[EZJSON_PARSER_INLINE_BUFFER];
        unsigned bufferSize;
        char *buffer;
        unsigned bufferPos;

        struct EzJSONBitStack stack;

        enum EzJSONParserState state;

        struct EzJSONToken token;

        char peeked;

        char hasPeeked;
        char hasToken;

        unsigned line;
        unsigned pos;
    };

    /// Initialize a new parser from the provided settings. The settings will be
    /// copied to internal memory.
    void *EzJSONParserInit(struct EzJSONParser *);

    /// Step the parser to the next token. Must be called once before the token
    /// becomes valid,
    void EzJSONParserNext(struct EzJSONParser *);

    /// Retrieve the current token. Will be null on error, EOF, or before the
    /// first call to EzJSONParserNext
    struct EzJSONToken *EzJSONParserToken(struct EzJSONParser *);

    /// Shut down the parser and free all memory
    void EzJSONParserDestroy(struct EzJSONParser *);

    /// Returns true if the parser reached an error state
    EzJSONBool EzJSONParserHasError(struct EzJSONParser *);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
#pragma once

#include "ezjson_common.h"

extern unsigned STACK_BIT_ARRAY;
extern unsigned STACK_BIT_OBJECT;

void stack_init(struct EzJSONBitStack *stack);
void stack_destroy(
    struct EzJSONBitStack *stack, void *userdata, EzJSONFree dealloc);

int stack_full(struct EzJSONBitStack *stack);
int stack_empty(struct EzJSONBitStack *stack);

int stack_top(struct EzJSONBitStack *stack);

void stack_grow(
    struct EzJSONBitStack *stack,
    void *userdata,
    EzJSONAlloc alloc,
    EzJSONFree dealloc);
void stack_push(
    struct EzJSONBitStack *stack,
    int bit,
    void *userdata,
    EzJSONAlloc alloc,
    EzJSONFree dealloc);
void stack_pop(struct EzJSONBitStack *stack);
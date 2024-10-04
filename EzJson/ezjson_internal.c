#include "ezjson_internal.h"

#include <memory.h>
#include <stdlib.h>

unsigned STACK_BIT_ARRAY  = 0;
unsigned STACK_BIT_OBJECT = 1;

void stack_init(struct EzJSONBitStack *stack)
{
    stack->data = stack->inlineData;
    stack->size = EZJSON_INLINE_STACK_SIZE;
    stack->top  = 0;
}

void stack_destroy(
    struct EzJSONBitStack *stack, void *userdata, EzJSONFree dealloc)
{
    if (stack->data != stack->inlineData)
    {
        if (dealloc)
        {
            dealloc(userdata, stack->data, stack->size);
        }
        else
        {
            free(stack->data);
        }
    }
}

int stack_full(struct EzJSONBitStack *stack)
{
    return stack->top == stack->size * sizeof(EzJSONStackChunk);
}

int stack_empty(struct EzJSONBitStack *stack)
{
    return stack->top == 0;
}

int stack_top(struct EzJSONBitStack *stack)
{
    const unsigned iChunk = (stack->top - 1) / sizeof(EzJSONStackChunk);
    const unsigned iBit   = (stack->top - 1) % sizeof(EzJSONStackChunk);

    return (stack->data[iChunk] & (1 << iBit)) != 0;
}

void stack_grow(
    struct EzJSONBitStack *stack,
    void *userdata,
    EzJSONAlloc alloc,
    EzJSONFree dealloc)
{
    const unsigned newSize    = (stack->size > 0) ? stack->size * 2 : 1;
    EzJSONStackChunk *newData = NULL;

    if (alloc)
    {
        newData = alloc(userdata, newSize);
    }
    else
    {
        newData = malloc(newSize);
    }

    memcpy(newData, stack->data, stack->size);

    if (stack->data != stack->inlineData)
    {
        if (dealloc)
        {
            dealloc(userdata, stack->data, stack->size);
        }
        else
        {
            free(stack->data);
        }
    }

    stack->data = newData;
    stack->size = newSize;
}

void stack_push(
    struct EzJSONBitStack *stack,
    int bit,
    void *userdata,
    EzJSONAlloc alloc,
    EzJSONFree dealloc)
{
    if (stack_full(stack))
    {
        stack_grow(stack, userdata, alloc, dealloc);
    }

    const unsigned iChunk = stack->top / sizeof(EzJSONStackChunk);
    const unsigned iBit   = stack->top % sizeof(EzJSONStackChunk);

    if (bit)
    {
        stack->data[iChunk] |= (1 << iBit);
    }
    else
    {
        stack->data[iChunk] &= ~(1 << iBit);
    }

    stack->top++;
}

void stack_pop(struct EzJSONBitStack *stack)
{
    if (!stack_empty(stack))
    {
        stack->top--;
    }
}

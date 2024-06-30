#pragma once

#include "defines.h"

typedef enum {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_STRING,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_DARRAY,
    MEMORY_TAG_HASHTABLE,
    MEMORY_TAG_RING_BUFFER,
    MEMORY_TAG_ARENA_ALLOCATOR,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_GAME,
    MEMORY_TAG_OPENGL,
    MEMORY_TAG_UI,
    MEMORY_TAG_NETWORK,
    MEMORY_TAG_COUNT
} memory_tag_e;

void* mem_alloc(u64 size, memory_tag_e tag);
void  mem_free(void* memory, u64 size, memory_tag_e tag);
void* mem_zero(void* block, u64 size);
void* mem_copy(void* dest, const void* source, u64 size);
void* mem_set(void* dest, i32 value, u64 size);
char* get_memory_usage_str(void);

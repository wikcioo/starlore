#pragma once

#include "defines.h"

#ifndef DEFAULT_ARENA_ALIGNMENT
#define DEFAULT_ARENA_ALIGNMENT (1 * sizeof(void *))
#endif

typedef struct {
    void *memory;
    u64   total_size;
    u64   current_offset;
    b8    is_memory_owner;
} arena_allocator_t;

void  arena_allocator_create         (u64 total_size, void *memory, arena_allocator_t *out_allocator);
void  arena_allocator_destroy        (arena_allocator_t *allocator);
void *arena_allocator_allocate       (arena_allocator_t *allocator, u64 size);
void *arena_allocator_allocate_align (arena_allocator_t *allocator, u64 size, u64 align);
void  arena_allocator_free_all       (arena_allocator_t *allocator);

#include "memutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "logger.h"
#include "asserts.h"

static const char *memory_tag_strings[MEMORY_TAG_COUNT] = {
    "unknown    ",
    "string     ",
    "array      ",
    "darray     ",
    "hashtable  ",
    "ring_buffer",
    "arena_alloc",
    "renderer   ",
    "game       ",
    "opengl     ",
    "ui         ",
    "network    "
};

typedef struct {
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_COUNT];
} memory_stats_t;

static memory_stats_t stats;

void* mem_alloc(u64 size, memory_tag_e tag)
{
    ASSERT(size > 0);
    ASSERT(tag >= MEMORY_TAG_UNKNOWN && tag < MEMORY_TAG_COUNT);

    stats.total_allocated += size;

    if (tag == MEMORY_TAG_UNKNOWN) {
        LOG_WARN("memory allocation of tag 'unknown' - consider re-tagging");
    }

    stats.tagged_allocations[tag] += size;

    void *memory = malloc(size);
    mem_zero(memory, size);
    return memory;
}

void mem_free(void* memory, u64 size, memory_tag_e tag)
{
    ASSERT(memory);
    ASSERT(size > 0);
    ASSERT(tag >= MEMORY_TAG_UNKNOWN && tag < MEMORY_TAG_COUNT);
    ASSERT(stats.tagged_allocations[tag] > 0);

    stats.tagged_allocations[tag] -= size;
    free(memory);
}

void* mem_zero(void* block, u64 size)
{
    ASSERT(block);
    return memset(block, 0, size);
}

void* mem_copy(void* dest, const void* source, u64 size)
{
    ASSERT(dest);
    ASSERT(source);
    return memcpy(dest, source, size);
}

void* mem_set(void* dest, i32 value, u64 size)
{
    ASSERT(dest);
    return memset(dest, value, size);
}

char* get_memory_usage_str(void)
{
    char buffer[1024] = "system memory usage:\n";
    u64 offset = strlen(buffer);
    for (i32 i = 0; i < MEMORY_TAG_COUNT; i++) {
        f32 usage;
        const char *unit = get_size_unit(stats.tagged_allocations[i], &usage);
        i32 length = snprintf(buffer + offset, 1024 - offset, "  %s: %.02f %s%c",
                              memory_tag_strings[i], usage, unit, i < MEMORY_TAG_COUNT-1 ? '\n' : ' ');
        offset += length;
    }

    return strdup(buffer);
}

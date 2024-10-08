#include "hashtable.h"

#include <memory.h>

#include "common/logger.h"
#include "common/strings.h"
#include "common/asserts.h"
#include "common/memory/memutils.h"

#define HEADER_SIZE     sizeof(u8)
#define FLAG_IS_SET_BIT 0

void hashtable_create(u64 element_size, u32 element_count, void* memory, hashtable_t* out_hashtable)
{
    ASSERT(memory && out_hashtable);
    ASSERT(element_size > 0 && element_count > 0);

    out_hashtable->memory = memory;
    out_hashtable->element_count = element_count;
    out_hashtable->element_size = element_size;
}

void hashtable_destroy(hashtable_t* table)
{
    ASSERT(table);
    mem_zero(table, sizeof(hashtable_t));
}

void hashtable_set(hashtable_t* table, const char* name, void* value)
{
    ASSERT(table && name && value);

    u64 sid = SID(name) % table->element_count;
    u8 *mem = (u8 *)table->memory + ((HEADER_SIZE + table->element_size) * sid);
    mem[0] |= BIT(FLAG_IS_SET_BIT);
    mem_copy(&mem[1], value, table->element_size);
}

void hashtable_get(hashtable_t* table, const char* name, void* out_value)
{
    ASSERT(table && name && out_value);

    u64 sid = SID(name) % table->element_count;
    u8 *mem = (u8 *)table->memory + ((HEADER_SIZE + table->element_size) * sid);
    mem_copy(out_value, &mem[1], table->element_size);
}

b8 hashtable_is_set(hashtable_t *table, const char *key)
{
    ASSERT(table && key);

    u64 sid = SID(key) % table->element_count;
    u8 *mem = (u8 *)table->memory + ((HEADER_SIZE + table->element_size) * sid);
    return mem[0] & BIT(FLAG_IS_SET_BIT);
}

void hashtable_fill(hashtable_t* table, void* value)
{
    ASSERT(table && value);

    for (u32 i = 0; i < table->element_count; ++i) {
        mem_copy((u8 *)table->memory + (table->element_size * i), value, table->element_size);
    }
}

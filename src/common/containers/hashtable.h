#pragma once

#include "defines.h"

typedef struct {
    u64 element_size;
    u32 element_count;
    void* memory;
} hashtable_t;

void hashtable_create     (u64 element_size, u32 element_count, void* memory, hashtable_t* out_hashtable);
void hashtable_destroy    (hashtable_t* table);
void hashtable_set        (hashtable_t* table, const char* name, void* value);
void hashtable_get        (hashtable_t* table, const char* name, void* out_value);
b8   hashtable_is_set     (hashtable_t *table, const char *key);
void hashtable_fill       (hashtable_t* table, void* value);

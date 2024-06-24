#pragma once

#include "defines.h"

#define SID(str) string_hash(str)

u64   string_hash       (const char *str);
void  string_insert_char(char *str, u32 index, char c);
char* string_trim       (char *str);

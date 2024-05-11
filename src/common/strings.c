#include "strings.h"

#include <ctype.h>
#include <memory.h>
#include <string.h>

#include "asserts.h"

void string_insert_char(char *str, u32 index, char c)
{
    ASSERT(str);

    u32 length = strlen(str);
    ASSERT(index < length);

    memcpy(str + index + 1, str + index, length - index);
    memcpy(str + index, &c, 1);
}

char* string_trim(char* str)
{
    ASSERT(str);

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str) {
        char* p = str;
        while (*p) {
            p++;
        }
        while (isspace((unsigned char)*(--p)))
            ;
        p[1] = '\0';
    }

    return str;
}

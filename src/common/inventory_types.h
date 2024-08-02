#pragma once

#include "global.h"

typedef enum {
    ITEM_CATEGORY_NONE,
    ITEM_CATEGORY_WEAPON,
    ITEM_CATEGORY_POTION,
    ITEM_CATEGORY_COUNT
} inventory_item_category_e;

typedef enum {
    ITEM_TYPE_NONE,
    ITEM_TYPE_SWORD,
    ITEM_TYPE_IMMUNITY_POTION,
    ITEM_TYPE_COUNT
} inventory_item_type_e;

typedef struct {
    inventory_item_category_e category;
    inventory_item_type_e type;
    char name[INVENTORY_MAX_ITEM_NAME_LENGTH];
} inventory_item_t;

typedef struct {
    inventory_item_t items[INVENTORY_INITIAL_CAPACITY];
    u32 selected_quick_access_item_index;
} inventory_t;

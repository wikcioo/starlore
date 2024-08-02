#pragma once

#include "event.h"
#include "common/inventory_types.h"

void inventory_create(inventory_t *inventory);
void inventory_destroy(inventory_t *inventory);

b8 inventory_add_item(inventory_t *inventory, inventory_item_t *item);
b8 inventory_add_item_at_index(inventory_t *inventory, inventory_item_t *item, u32 index);

void inventory_load_resources(void);
void inventory_render(inventory_t *inventory);

b8 inventory_mouse_button_pressed_event_callback  (event_code_e code, event_data_t data);
b8 inventory_mouse_button_released_event_callback (event_code_e code, event_data_t data);
b8 inventory_key_pressed_event_callback           (event_code_e code, event_data_t data);

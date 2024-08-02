#include "inventory.h"

#include <string.h>

#include "config.h"
#include "input.h"
#include "renderer.h"
#include "color_palette.h"
#include "common/asserts.h"
#include "common/logger.h"
#include "common/input_codes.h"
#include "common/memory/memutils.h"
#include "common/containers/darray.h"

#define ITEM_NOT_PICKED_INDEX -1

extern vec2 main_window_size;
extern inventory_t player_inventory;

static const vec2 item_size = (vec2){ .x = 50.0f, .y = 50.0f };
static const f32 item_x_pad = 5.0f;
static const f32 inventory_y_offset = 10.0f;

static texture_t inventory_spritesheet;
static vec2 inventory_tex_coord[ITEM_TYPE_COUNT][TEX_COORD_COUNT];

typedef struct {
    b8  opened;
    i32 item_picked_index;
} inventory_context_t;

static inventory_context_t context;

static void inventory_swap_items(inventory_t *inventory, u32 src_index, u32 dest_index)
{
    ASSERT(inventory);
    ASSERT(src_index >= 0 && src_index < INVENTORY_INITIAL_CAPACITY);
    ASSERT(dest_index >= 0 && dest_index < INVENTORY_INITIAL_CAPACITY);
    ASSERT(inventory->items[src_index].type != ITEM_TYPE_NONE);

    if (src_index == dest_index) {
        return;
    }

    if (inventory->items[dest_index].type == ITEM_TYPE_NONE) {
        mem_copy(&inventory->items[dest_index], &inventory->items[src_index], sizeof(inventory_item_t));
        mem_zero(&inventory->items[src_index], sizeof(inventory_item_t));
    } else {
        inventory_item_t temp = inventory->items[dest_index];
        mem_copy(&inventory->items[dest_index], &inventory->items[src_index], sizeof(inventory_item_t));
        mem_copy(&inventory->items[src_index], &temp, sizeof(inventory_item_t));
    }
}

void inventory_create(inventory_t *inventory)
{
    ASSERT(inventory);

    mem_zero(inventory, sizeof(inventory_t));

    context.opened = false;
    context.item_picked_index = ITEM_NOT_PICKED_INDEX;

#if defined(DEBUG) && 1
    inventory_item_t item1 = {0};
    item1.category = ITEM_CATEGORY_WEAPON;
    item1.type = ITEM_TYPE_SWORD;
    strncpy(item1.name, "foo", INVENTORY_MAX_ITEM_NAME_LENGTH);
    ASSERT(inventory_add_item(inventory, &item1));

    inventory_item_t item2 = {0};
    item2.category = ITEM_CATEGORY_POTION;
    item2.type = ITEM_TYPE_IMMUNITY_POTION;
    strncpy(item2.name, "bar", INVENTORY_MAX_ITEM_NAME_LENGTH);
    ASSERT(inventory_add_item(inventory, &item2));

    inventory->selected_quick_access_item_index = 0;
#endif
}

void inventory_destroy(inventory_t *inventory)
{
    ASSERT(inventory);
    texture_destroy(&inventory_spritesheet);
}

void inventory_load_resources(void)
{
    // TODO: refactor by using a global asset manager
    static const f32 item_width_px = 16.0f;
    static const f32 item_height_px = 16.0f;

    texture_create_from_path("assets/textures/items/inventory.png", &inventory_spritesheet);
    f32 item_width_uv = item_width_px / (f32)inventory_spritesheet.width;
    f32 item_height_uv = item_height_px / (f32)inventory_spritesheet.height;

    f32 x, y;

    // silver sword
    x = 1 * item_width_uv;
    y = 13 * item_height_uv;
    inventory_tex_coord[ITEM_TYPE_SWORD][0] = vec2_create(x, y);
    inventory_tex_coord[ITEM_TYPE_SWORD][1] = vec2_create(x, y + item_height_uv);
    inventory_tex_coord[ITEM_TYPE_SWORD][2] = vec2_create(x + item_width_uv, y + item_height_uv);
    inventory_tex_coord[ITEM_TYPE_SWORD][3] = vec2_create(x + item_width_uv, y);

    // potion
    x = 1 * item_width_uv;
    y = 0 * item_height_uv;
    inventory_tex_coord[ITEM_TYPE_IMMUNITY_POTION][0] = vec2_create(x, y);
    inventory_tex_coord[ITEM_TYPE_IMMUNITY_POTION][1] = vec2_create(x, y + item_height_uv);
    inventory_tex_coord[ITEM_TYPE_IMMUNITY_POTION][2] = vec2_create(x + item_width_uv, y + item_height_uv);
    inventory_tex_coord[ITEM_TYPE_IMMUNITY_POTION][3] = vec2_create(x + item_width_uv, y);
}

b8 inventory_add_item(inventory_t *inventory, inventory_item_t *item)
{
    ASSERT(inventory && item);

    for (u32 i = 0; i < INVENTORY_INITIAL_CAPACITY; i++) {
        if (inventory->items[i].type == ITEM_TYPE_NONE) {
            // found free slot
            return inventory_add_item_at_index(inventory, item, i);
        }
    }

    return false;
}

b8 inventory_add_item_at_index(inventory_t *inventory, inventory_item_t *item, u32 index)
{
    ASSERT(inventory && item)
    ASSERT(index >= 0 && index < INVENTORY_INITIAL_CAPACITY);

    if (inventory->items[index].type != ITEM_TYPE_NONE) {
        LOG_WARN("tried adding item '%s' to inventory at index %u but item '%s' already exists at that index",
                 item->name ,index, inventory->items[index].name);
        return false;
    }

    mem_copy(&inventory->items[index], item, sizeof(inventory_item_t));
    return true;
}

void inventory_render(inventory_t *inventory)
{
    ASSERT(inventory);

    vec2 item_position = vec2_create(
        -INVENTORY_MAX_QUICK_ACCESS_ITEMS * 0.5f * (item_size.x + item_x_pad) + item_x_pad * 0.5f + item_size.x * 0.5f,
        -main_window_size.y * 0.5f + item_size.y * 0.5f + inventory_y_offset
    );

    for (u32 i = 0; i < INVENTORY_MAX_QUICK_ACCESS_ITEMS; i++) {
        renderer_draw_quad_color(item_position, item_size, 0.0f, COLOR_MILK, 0.7f);
        renderer_draw_rect(item_position,
                           item_size,
                           inventory->selected_quick_access_item_index == i ? COLOR_GOLDEN_YELLOW : COLOR_MIDNIGHT_BLUE,
                           1.0f);

        if (inventory->items[i].type != ITEM_TYPE_NONE && i != context.item_picked_index) {
            vec2 uv[4];
            mem_copy(uv, &inventory_tex_coord[inventory->items[i].type], sizeof(vec2) * 4);
            renderer_draw_quad_sprite_uv(item_position, item_size, 0.0f, &inventory_spritesheet, uv);
        }
        item_position.x += item_size.x + item_x_pad;
    }

    if (context.item_picked_index != ITEM_NOT_PICKED_INDEX) {
        // Draw item attached to cursor
        vec2 mp = input_get_mouse_position();
        // move origin of mouse position to the center of the screen
        mp.x = mp.x - main_window_size.x * 0.5f;
        mp.y = main_window_size.y * 0.5f - mp.y;

        vec2 uv[4];
        mem_copy(uv, &inventory_tex_coord[inventory->items[context.item_picked_index].type], sizeof(vec2) * 4);
        renderer_draw_quad_sprite_uv(mp, item_size, 0.0f, &inventory_spritesheet, uv);
    }
}

static b8 rect_contains(vec2 pos /* center */, vec2 size, vec2 p)
{
    return p.x >= pos.x - size.x * 0.5f && p.x <= pos.x + size.x * 0.5f && p.y >= pos.y - size.y * 0.5f && p.y <= pos.y + size.y * 0.5f;
}

b8 inventory_mouse_button_pressed_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];
    if (btn != MOUSEBUTTON_LEFT) {
        return false;
    }

    vec2 mp = input_get_mouse_position();
    // move origin of mouse position to the center of the screen
    mp.x = mp.x - main_window_size.x * 0.5f;
    mp.y = main_window_size.y * 0.5f - mp.y;

    vec2 item_position = vec2_create(
        -INVENTORY_MAX_QUICK_ACCESS_ITEMS * 0.5f * (item_size.x + item_x_pad) + item_x_pad * 0.5f + item_size.x * 0.5f,
        -main_window_size.y * 0.5f + item_size.y * 0.5f + inventory_y_offset
    );

    for (u32 i = 0; i < INVENTORY_MAX_QUICK_ACCESS_ITEMS; i++) {
        if (rect_contains(item_position, item_size, mp) && player_inventory.items[i].type != ITEM_TYPE_NONE) {
            context.item_picked_index = i;
            return true;
        }

        item_position.x += item_size.x + item_x_pad;
    }

    return false;
}

b8 inventory_mouse_button_released_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];
    if (btn != MOUSEBUTTON_LEFT || context.item_picked_index == ITEM_NOT_PICKED_INDEX) {
        return false;
    }

    vec2 mp = input_get_mouse_position();
    // move origin of mouse position to the center of the screen
    mp.x = mp.x - main_window_size.x * 0.5f;
    mp.y = main_window_size.y * 0.5f - mp.y;

    vec2 item_position = vec2_create(
        -INVENTORY_MAX_QUICK_ACCESS_ITEMS * 0.5f * (item_size.x + item_x_pad) + item_x_pad * 0.5f + item_size.x * 0.5f,
        -main_window_size.y * 0.5f + item_size.y * 0.5f + inventory_y_offset
    );

    for (u32 i = 0; i < INVENTORY_MAX_QUICK_ACCESS_ITEMS; i++) {
        if (rect_contains(item_position, item_size, mp)) {
            inventory_swap_items(&player_inventory, context.item_picked_index, i);
            context.item_picked_index = ITEM_NOT_PICKED_INDEX;
            return true;
        }

        item_position.x += item_size.x + item_x_pad;
    }

    // Mouse released outside of quick access inventory, so cancel picking an item
    context.item_picked_index = ITEM_NOT_PICKED_INDEX;
    return true;
}

b8 inventory_key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    u16 upper_limit = KEYCODE_D1 + math_min(INVENTORY_MAX_QUICK_ACCESS_ITEMS-1, KEYCODE_D9-KEYCODE_D1);
    if (key >= KEYCODE_D1 && key <= upper_limit) {
        player_inventory.selected_quick_access_item_index = key - KEYCODE_D1;
        return true;
    }

    return false;
}

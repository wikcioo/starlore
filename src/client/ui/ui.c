#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "color_palette.h"
#include "common/maths.h"
#include "common/logger.h"
#include "common/strings.h"
#include "common/asserts.h"
#include "common/containers/stack.h"
#include "common/containers/darray.h"
#include "common/containers/hashtable.h"
#include "common/containers/ring_buffer.h"
#include "common/memory/arena_allocator.h"
#include "common/memory/memutils.h"

// TODO: make configurable and create ui color pallete
#define TEXT_COLOR                          vec3_create(0.93f, 0.89f, 0.75f)
#define BORDER_COLOR                        vec3_create(0.76f, 0.60f, 0.42f)
#define BACKGROUND_COLOR                    vec3_create(0.19f, 0.15f, 0.13f)
#define BUTTON_BACKGROUND_COLOR             vec3_create(0.50f, 0.29f, 0.07f)
#define BUTTON_HOVER_COLOR                  vec3_create(0.65f, 0.42f, 0.13f)
#define BUTTON_CLICK_COLOR                  vec3_create(0.79f, 0.52f, 0.17f)
#define CHECKBOX_BACKGROUND_COLOR           vec3_create(0.50f, 0.29f, 0.07f)
#define CHECKBOX_BORDER_COLOR               vec3_create(0.76f, 0.60f, 0.42f)
#define CHECKBOX_CHECKED_COLOR              vec3_create(0.60f, 0.80f, 0.20f)
#define CHECKBOX_HOVER_BACKGROUND_COLOR     vec3_create(0.65f, 0.42f, 0.13f)
#define RADIO_BUTTON_BACKGROUND_COLOR       vec3_create(0.50f, 0.29f, 0.07f)
#define RADIO_BUTTON_BORDER_COLOR           vec3_create(0.76f, 0.60f, 0.42f)
#define RADIO_BUTTON_SELECTED_COLOR         vec3_create(0.60f, 0.80f, 0.20f)
#define RADIO_BUTTON_HOVER_BACKGROUND_COLOR vec3_create(0.65f, 0.42f, 0.13f)
#define SLIDER_BACKGROUND_COLOR             vec3_create(0.50f, 0.29f, 0.07f)
#define SLIDER_BUTTON_COLOR                 vec3_create(0.60f, 0.80f, 0.20f)
#define SLIDER_BUTTON_HOVER_COLOR           vec3_create(0.70f, 0.90f, 0.25f)
#define SLIDER_BUTTON_ACTIVE_COLOR          vec3_create(0.80f, 0.95f, 0.04f)
#define INPUT_BOX_BACKGROUND_COLOR          vec3_create(0.50f, 0.29f, 0.07f)
#define INPUT_BOX_HOVER_COLOR               vec3_create(0.65f, 0.42f, 0.13f)
#define INPUT_BOX_BORDER_COLOR              vec3_create(0.76f, 0.60f, 0.42f)
#define INPUT_BOX_CURSOR_COLOR              vec3_create(0.60f, 0.80f, 0.20f)

#define UI_INVALID_ID       (0)
#define WINDOW_INVALID_IDX  (-1)

#define MAX_INPUT_BOX_COUNT             64
#define INPUT_BUFFER_CAPACITY           32
#define INPUT_BUFFER_MAX_PROCESSED_KEYS 8

#define MIN_WIN_WIDTH  200
#define MIN_WIN_HEIGHT 200

#define FRAME_ALLOCATOR_SIZE KiB(1)

// TODO: to be defined here
extern camera_t ui_camera;

extern vec2 main_window_size;

typedef u64 ui_id;

typedef struct {
    vec2 position;
    vec2 size;
    vec2 last_widget_size;
    b8   in_line;
} ui_layout_t;

typedef struct {
    font_atlas_size_e fa_size;
    f32 win_inner_pad;
    f32 win_title_y_pad;
    f32 widget_x_pad;
    f32 widget_y_pad;
    f32 checkbox_x_pad;
    f32 radiobutton_x_pad;
    f32 btn_pad;
    f32 win_close_btn_scale;
    f32 separator_height;
    f32 separator_thickness;
    f32 slider_y_pad;
    f32 slider_text_x_pad;
    f32 slider_btn_y_pad;
    f32 input_box_label_x_pad;
    f32 input_box_text_x_pad;
    f32 input_box_y_pad;
    f32 input_box_cursor_width;
    f32 input_box_cursor_y_pad;
} ui_config_t;

typedef struct {
    ui_id id;
    vec2 position; // top-left
    vec2 size;
    f32  alpha;
    b8   draggable : 1;
    b8   resizable : 1;
    ui_layout_t layout;
} ui_window_t;

typedef struct {
    u32 text_offset;
    u32 cursor_offset;
    b8  content_flushed_last_frame;
} input_box_state_t;

typedef struct {
    ui_id hot_id;
    ui_id active_id;

    void *input_box_state_memory;
    hashtable_t input_box_state;
    b8   input_active                  : 1;
    b8   input_has_space               : 1;

    b8   mouse_left_pressed            : 1;
    b8   mouse_left_pressed_last_frame : 1;
    b8   mouse_right_pressed           : 1;
    vec2 mouse_screen_pos;
    vec2 mouse_screen_pos_last_frame;

    i32 window_hovered_idx;
    i32 window_focused_idx;
    i32 window_captured_idx;
    i32 window_resizing_idx;
    i32 window_current_idx;
    ui_window_t *windows;

    ui_config_t config;
} ui_t;

static ui_t ui;
static stack_t id_stack;
static u32 *input_buffer;
static font_atlas_size_e default_font_size = FA32;
static arena_allocator_t frame_allocator;

static vec2 ui_layout_get_position(ui_window_t *window)
{
    if (!window->layout.in_line) {
        window->layout.position.x = ui.config.win_inner_pad;
    } else {
        window->layout.position.y -= window->layout.last_widget_size.y + ui.config.widget_y_pad;
        window->layout.in_line = false;
    }
    return window->layout.position;
}

static void ui_layout_add_widget(ui_window_t *window, vec2 size)
{
    window->layout.position.x += size.x + ui.config.widget_x_pad;
    window->layout.position.y += size.y + ui.config.widget_y_pad;
    window->layout.last_widget_size = size;
}

static b8 rect_contains(vec2 pos /* top-left */, vec2 size, vec2 p)
{
    return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y && p.y <= pos.y + size.y;
}

static ui_id get_widget_id(const char *str)
{
    ui_id top_id;
    b8 result = stack_peek(&id_stack, &top_id);
    UNUSED(result); // Prevents compiler warning in release build
    ASSERT(result);
    return SID(str) + top_id; // Allow overflow
}

void ui_init(void)
{
    ui.hot_id    = UI_INVALID_ID;
    ui.active_id = UI_INVALID_ID;

    ui.window_hovered_idx  = WINDOW_INVALID_IDX;
    ui.window_focused_idx  = WINDOW_INVALID_IDX;
    ui.window_captured_idx = WINDOW_INVALID_IDX;
    ui.window_resizing_idx = WINDOW_INVALID_IDX;
    ui.window_current_idx  = WINDOW_INVALID_IDX;
    ui.windows = darray_reserve(5, sizeof(ui_window_t));

    ui.config.fa_size                   = default_font_size;
    ui.config.win_inner_pad             = 3.0f;
    ui.config.win_title_y_pad           = 3.0f;
    ui.config.widget_x_pad              = 5.0f;
    ui.config.widget_y_pad              = 5.0f;
    ui.config.checkbox_x_pad            = 5.0f;
    ui.config.radiobutton_x_pad         = 5.0f;
    ui.config.btn_pad                   = 3.0f;
    ui.config.win_close_btn_scale       = 0.8f;
    ui.config.separator_height          = 10.0f;
    ui.config.separator_thickness       = 2.0f;
    ui.config.slider_y_pad              = 2.0f;
    ui.config.slider_text_x_pad         = 5.0f;
    ui.config.slider_btn_y_pad          = 2.0f;
    ui.config.input_box_label_x_pad     = 5.0f;
    ui.config.input_box_text_x_pad      = 2.0f;
    ui.config.input_box_y_pad           = 2.0f;
    ui.config.input_box_cursor_width    = 2.0f;
    ui.config.input_box_cursor_y_pad    = 3.0f;

    stack_create(sizeof(ui_id), &id_stack);
    input_buffer = ring_buffer_reserve(INPUT_BUFFER_CAPACITY, sizeof(u32));

    ui.input_box_state_memory = mem_alloc(MAX_INPUT_BOX_COUNT * sizeof(input_box_state_t), MEMORY_TAG_HASHTABLE);
    hashtable_create(sizeof(input_box_state_t), MAX_INPUT_BOX_COUNT, ui.input_box_state_memory, &ui.input_box_state);

    arena_allocator_create(FRAME_ALLOCATOR_SIZE, 0, &frame_allocator);
}

void ui_shutdown(void)
{
    darray_destroy(ui.windows);
    stack_destroy(&id_stack);
    ring_buffer_destroy(input_buffer);

    mem_free(ui.input_box_state_memory, MAX_INPUT_BOX_COUNT * sizeof(input_box_state_t), MEMORY_TAG_HASHTABLE);
    hashtable_destroy(&ui.input_box_state);

    arena_allocator_destroy(&frame_allocator);
}

void ui_begin_frame(void)
{
    stack_clear(&id_stack);
}

void ui_end_frame(void)
{
    ui.mouse_left_pressed_last_frame = ui.mouse_left_pressed;
    ui.mouse_screen_pos_last_frame = ui.mouse_screen_pos;

    arena_allocator_free_all(&frame_allocator);
}

void ui_begin(const char *name, b8 *visible)
{
    ui_window_config_t config = {
        .position  = { .x = 100.0f, .y = 100.0f },
        .size      = { .x = MIN_WIN_WIDTH, .y = MIN_WIN_HEIGHT },
        .draggable = true,
        .resizable = true,
        .font_size = default_font_size
    };
    ui_begin_conf(name, &config, visible);
}

void ui_begin_conf(const char *name, ui_window_config_t *config, b8 *visible)
{
    ASSERT_MSG(ui.window_current_idx == WINDOW_INVALID_IDX, "called ui_begin() while already in a ui_begin() block!");

    ui.config.fa_size = config->font_size;

    b8  window_exists = false;
    u64 window_id     = SID(name);
    u64 window_idx    = WINDOW_INVALID_IDX;
    u64 windows_count = darray_length(ui.windows);

    for (u64 i = 0; i < windows_count; i++) {
        if (ui.windows[i].id == window_id) {
            window_exists = true;
            window_idx = i;
            break;
        }
    }

    if (!window_exists) {
        // TODO: Spawn new window in a non-fully overlapping position
        ui_window_t window = {
            .id        = window_id,
            .position  = config->position,
            .size      = config->size,
            .draggable = config->draggable,
            .resizable = config->resizable
        };
        window_idx = windows_count;
        darray_push(ui.windows, window);
        LOG_TRACE("created new ui window id=%llu position=(%.02f,%.02f) size=(%.02f,%.02f)",
                  window.id, window.position.x, window.position.y, window.size.x, window.size.y);
    }

    stack_push(&id_stack, &window_id);

    ui_window_t *win = &ui.windows[window_idx];
    ui.window_current_idx = window_idx;

    win->layout.position = vec2_create(ui.config.win_inner_pad, ui.config.win_inner_pad);
    win->layout.size     = vec2_zero();
    win->layout.in_line  = false;
    win->layout.last_widget_size = vec2_zero();

    static const f32 resizing_alpha = 0.4f;
    win->alpha = 1.0f - (ui.window_resizing_idx == window_idx) * (1.0f - resizing_alpha);

    renderer_begin_scene(&ui_camera);

    // render window background
    vec2 win_render_pos = vec2_create(
        (win->position.x - main_window_size.x / 2.0f) + win->size.x / 2.0f,
        (main_window_size.y / 2.0f - win->position.y) - win->size.y / 2.0f
    );
    renderer_draw_quad_color(win_render_pos, win->size, 0.0f, BACKGROUND_COLOR, win->alpha);

    // render border if focused
    if (window_idx == ui.window_focused_idx) {
        renderer_draw_rect(win_render_pos, win->size, BORDER_COLOR, 1.0f);
    }

    { // render window header
        // title
        u32 text_height = renderer_get_font_height(ui.config.fa_size);
        u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);
        vec2 title_render_pos = vec2_create(
            win_render_pos.x - win->size.x / 2.0f + win->layout.position.x,
            win_render_pos.y + win->size.y / 2.0f - text_bearing_y - win->layout.position.y
        );
        renderer_draw_text(name, ui.config.fa_size, title_render_pos, 1.0f, TEXT_COLOR, win->alpha);

        win->layout.position.y += text_height + ui.config.win_title_y_pad;

        // close button
        vec2 close_btn_size = vec2_create(text_height * ui.config.win_close_btn_scale, text_height * ui.config.win_close_btn_scale);
        vec2 close_btn_render_pos = vec2_create(
            win_render_pos.x + win->size.x / 2.0f - ui.config.win_inner_pad - close_btn_size.x / 2.0f,
            win_render_pos.y + win->size.y / 2.0f - ui.config.win_inner_pad - close_btn_size.y / 2.0f
        );

        vec2 close_btn_screen_pos = vec2_create(
            close_btn_render_pos.x - close_btn_size.x / 2.0f + main_window_size.x / 2.0f,
            main_window_size.y / 2.0f - close_btn_render_pos.y - close_btn_size.y / 2.0f
        );

        if (rect_contains(close_btn_screen_pos, close_btn_size, ui.mouse_screen_pos)) {
            close_btn_size.x = text_height * (ui.config.win_close_btn_scale + 0.1f);
            close_btn_size.y = text_height * (ui.config.win_close_btn_scale + 0.1f);
            if (ui.mouse_left_pressed) {
                // Prevents the window from being dragged when doing click-and-hold of the close button
                ui.window_captured_idx = WINDOW_INVALID_IDX;
            } else if (ui.mouse_left_pressed_last_frame) {
                *visible = false;
            }
        }

        vec2 bl = vec2_create(close_btn_render_pos.x - close_btn_size.x * 0.5f,
                              close_btn_render_pos.y - close_btn_size.y * 0.5f);
        vec2 tr = vec2_create(close_btn_render_pos.x + close_btn_size.x * 0.5f,
                              close_btn_render_pos.y + close_btn_size.y * 0.5f);
        vec2 br = vec2_create(close_btn_render_pos.x + close_btn_size.x * 0.5f,
                              close_btn_render_pos.y - close_btn_size.y * 0.5f);
        vec2 tl = vec2_create(close_btn_render_pos.x - close_btn_size.x * 0.5f,
                              close_btn_render_pos.y + close_btn_size.y * 0.5f);
        renderer_draw_line(tr, bl, TEXT_COLOR, win->alpha);
        renderer_draw_line(tl, br, TEXT_COLOR, win->alpha);
    }

    // Make window header separator as high as its thickness, so that there is no extra margin
    f32 old_height = ui.config.separator_height;
    ui.config.separator_height = ui.config.separator_thickness;
    ui_separator();
    ui.config.separator_height = old_height;

    // render window dimensions overlay when resizing
    if (ui.window_resizing_idx == window_idx) {
        renderer_draw_quad_color(win_render_pos, win->size, 0.0f, COLOR_BLACK, 0.8f);

        char size_buf[32] = {0};
        snprintf(size_buf, sizeof(size_buf), "%ix%i", (i32)win->size.x, (i32)win->size.y);
        u32 size_text_width = renderer_get_font_width(ui.config.fa_size) * strlen(size_buf);
        u32 size_text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);
        vec2 size_text_render_pos = vec2_create(
            math_round(win_render_pos.x - size_text_width / 2.0f),
            math_round(win_render_pos.y - size_text_bearing_y / 2.0f)
        );
        renderer_draw_text(size_buf, ui.config.fa_size, size_text_render_pos, 1.0f, TEXT_COLOR, 1.0f);
    }
}

void ui_end(void)
{
    ASSERT_MSG(ui.window_current_idx != WINDOW_INVALID_IDX, "called ui_end() without a previous call to ui_begin()!");
    stack_pop(&id_stack, 0);
    ui.window_current_idx = WINDOW_INVALID_IDX;
    renderer_end_scene();
}

static i32 get_num_lines(const char *text)
{
    i32 num_lines = 1;
    const char *c = text;
    while (*c) {
        if (*c == '\n') {
            num_lines++;
        }
        c++;
    }
    return num_lines;
}

void ui_text(const char *text)
{
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 text_pos = ui_layout_get_position(win);

    vec2 text_size = vec2_create(text_width, text_height * get_num_lines(text));
    ui_layout_add_widget(win, text_size);

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 text_render_pos = vec2_create(
        win_render_pos.x + text_pos.x,
        win_render_pos.y - text_pos.y - text_bearing_y
    );
    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, win->alpha);
}

b8 ui_button(const char *text)
{
    ui_id id = get_widget_id(text);
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 btn_pos = ui_layout_get_position(win);
    vec2 btn_size = vec2_create(text_width + ui.config.btn_pad * 2.0f, text_height + ui.config.btn_pad * 2.0f);
    ui_layout_add_widget(win, btn_size);

    b8 clicked = false;
    vec3 btn_color = BUTTON_BACKGROUND_COLOR;

    vec2 btn_screen_pos = vec2_create(
        win->position.x + btn_pos.x,
        win->position.y + btn_pos.y
    );
    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
            if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
                clicked = true;
            }
        }
    } else {
        if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed && ui.active_id == UI_INVALID_ID) {
                ui.active_id = id;
            } else {
                ui.hot_id = id;
            }
        } else {
            ui.hot_id = UI_INVALID_ID;
        }
    }

    if (ui.active_id == id) {
        btn_color = BUTTON_CLICK_COLOR;
    } else if (ui.hot_id == id) {
        btn_color = BUTTON_HOVER_COLOR;
    }

    // render button background
    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 btn_render_pos = vec2_create(
        win_render_pos.x + btn_pos.x + btn_size.x / 2.0f,
        win_render_pos.y - btn_pos.y - btn_size.y / 2.0f
    );
    renderer_draw_quad_color(btn_render_pos, btn_size, 0.0f, btn_color, win->alpha);

    // render button text content
    vec2 btn_text_render_pos = vec2_create(
        btn_render_pos.x - text_width / 2.0f,
        btn_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_text(text, ui.config.fa_size, btn_text_render_pos, 1.0f, TEXT_COLOR, win->alpha);

    return clicked;
}

b8 ui_checkbox(const char *text, b8 *is_checked)
{
    ui_id id = get_widget_id(text);
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 box_pos = ui_layout_get_position(win);
    vec2 box_size = vec2_create(text_height, text_height);

    vec2 widget_size = vec2_create(
        box_size.x + text_width + ui.config.checkbox_x_pad,
        box_size.y
    );

    ui_layout_add_widget(win, widget_size);

    vec3 box_color = CHECKBOX_BACKGROUND_COLOR;

    vec2 box_screen_pos = vec2_create(
        win->position.x + box_pos.x,
        win->position.y + box_pos.y
    );

    b8 state_changed = false;
    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
            if (rect_contains(box_screen_pos, box_size, ui.mouse_screen_pos)) {
                *is_checked = !(*is_checked);
                state_changed = true;
            }
        }
    } else {
        if (rect_contains(box_screen_pos, box_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed && ui.active_id == UI_INVALID_ID) {
                ui.active_id = id;
            } else {
                ui.hot_id = id;
            }
        } else {
            ui.hot_id = UI_INVALID_ID;
        }
    }

    if (*is_checked) {
        box_color = CHECKBOX_CHECKED_COLOR;
    } else if (ui.hot_id == id) {
        box_color = CHECKBOX_HOVER_BACKGROUND_COLOR;
    }

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 box_render_pos = vec2_create(
        win_render_pos.x + box_pos.x + box_size.x / 2.0f,
        win_render_pos.y - box_pos.y - box_size.y / 2.0f
    );

    vec2 text_render_pos = vec2_create(
        box_render_pos.x + box_size.x / 2.0f + ui.config.checkbox_x_pad,
        box_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_quad_color(box_render_pos, box_size, 0.0f, box_color, win->alpha);
    renderer_draw_rect(box_render_pos, box_size, CHECKBOX_BORDER_COLOR, win->alpha);

    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, win->alpha);

    return state_changed;
}

b8 ui_radiobutton(const char *text, i32 *selected_id, i32 self_id)
{
    ui_id id = get_widget_id(text);
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 btn_pos = ui_layout_get_position(win);
    vec2 btn_size = vec2_create(text_height, text_height);

    vec2 widget_size = vec2_create(
        btn_size.x + text_width + ui.config.radiobutton_x_pad,
        btn_size.y
    );

    ui_layout_add_widget(win, widget_size);

    vec3 btn_color = RADIO_BUTTON_BACKGROUND_COLOR;

    vec2 btn_screen_pos = vec2_create(
        win->position.x + btn_pos.x,
        win->position.y + btn_pos.y
    );

    b8 state_changed = false;
    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
            if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
                state_changed = *selected_id != self_id;
                *selected_id = self_id;
            }
        }
    } else {
        if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed && ui.active_id == UI_INVALID_ID) {
                ui.active_id = id;
            } else {
                ui.hot_id = id;
            }
        } else {
            ui.hot_id = UI_INVALID_ID;
        }
    }

    if (self_id == *selected_id) {
        btn_color = RADIO_BUTTON_SELECTED_COLOR;
    } else if (ui.hot_id == id) {
        btn_color = RADIO_BUTTON_HOVER_BACKGROUND_COLOR;
    }

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 btn_render_pos = vec2_create(
        win_render_pos.x + btn_pos.x + btn_size.x / 2.0f,
        win_render_pos.y - btn_pos.y - btn_size.y / 2.0f
    );

    vec2 text_render_pos = vec2_create(
        btn_render_pos.x + btn_size.x / 2.0f + ui.config.radiobutton_x_pad,
        btn_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_circle(btn_render_pos, btn_size.x / 2.0f, btn_color, win->alpha);
    renderer_draw_circle_thick(btn_render_pos, btn_size.x / 2.0f, 0.2f, RADIO_BUTTON_BORDER_COLOR, win->alpha);

    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, win->alpha);

    return state_changed;
}

void ui_slider_float(const char *text, f32 *value, f32 low, f32 high)
{
    ui_id id = get_widget_id(text);
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 slider_pos = ui_layout_get_position(win);
    vec2 slider_size = vec2_create(
        win->size.x - ui.config.win_inner_pad * 2.0f - text_width - ui.config.slider_text_x_pad,
        text_height + ui.config.slider_y_pad * 2.0f
    );

    vec2 widget_size = vec2_create(
        slider_size.x + text_width + ui.config.slider_text_x_pad,
        slider_size.y
    );

    ui_layout_add_widget(win, widget_size);

    vec3 btn_color = SLIDER_BUTTON_COLOR;

    vec2 btn_size = vec2_create(
        slider_size.y * 0.6f,
        slider_size.y - (ui.config.slider_btn_y_pad * 2.0f)
    );
    f32 t = (*value - low) / (high - low);
    f32 btn_x_offset = t * (slider_size.x - btn_size.x) + (btn_size.x / 2.0f);
    vec2 btn_pos = vec2_create(
        slider_pos.x - btn_size.x / 2.0f + btn_x_offset,
        slider_pos.y + ui.config.slider_btn_y_pad
    );

    vec2 btn_screen_pos = vec2_create(
        win->position.x + btn_pos.x,
        win->position.y + btn_pos.y
    );

    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
        } else {
            f32 dx = ui.mouse_screen_pos.x - ui.mouse_screen_pos_last_frame.x;
            if (dx != 0.0f) {
                f32 dt = dx / (slider_size.x - btn_size.x);
                *value += dt * (high - low);
                if (*value < low) {
                    *value = low;
                } else if (*value > high) {
                    *value = high;
                }
            }
        }
    } else {
        if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed && ui.active_id == UI_INVALID_ID) {
                ui.active_id = id;
            } else {
                ui.hot_id = id;
            }
        } else {
            ui.hot_id = UI_INVALID_ID;
        }
    }

    if (ui.active_id == id) {
        btn_color = SLIDER_BUTTON_ACTIVE_COLOR;
    } else if (ui.hot_id == id) {
        btn_color = SLIDER_BUTTON_HOVER_COLOR;
    }

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 slider_render_pos = vec2_create(
        win_render_pos.x + slider_pos.x + slider_size.x / 2.0f,
        win_render_pos.y - slider_pos.y - slider_size.y / 2.0f
    );

    vec2 btn_render_pos = vec2_create(
        slider_render_pos.x - slider_size.x / 2.0f + btn_x_offset,
        slider_render_pos.y
    );

    vec2 text_render_pos = vec2_create(
        math_round(slider_render_pos.x + slider_size.x / 2.0f + ui.config.slider_text_x_pad),
        math_round(slider_render_pos.y - text_bearing_y / 2.0f)
    );

    char value_buf[32] = {0};
    snprintf(value_buf, sizeof(value_buf), "%.04f", *value);
    u32 value_width = renderer_get_font_width(ui.config.fa_size) * strlen(value_buf);
    vec2 value_render_pos = vec2_create(
        math_round(slider_render_pos.x - value_width / 2.0f),
        math_round(slider_render_pos.y - text_bearing_y / 2.0f)
    );

    renderer_draw_quad_color(slider_render_pos, slider_size, 0.0f, SLIDER_BACKGROUND_COLOR, win->alpha);
    renderer_draw_quad_color(btn_render_pos, btn_size, 0.0f, btn_color, win->alpha);
    renderer_draw_text(value_buf, ui.config.fa_size, value_render_pos, 1.0f, TEXT_COLOR, win->alpha);
    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, win->alpha);
}

b8 ui_input_text(const char *label, char *text, u32 max_size)
{
    ui_id id = get_widget_id(label);
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    u32 num_chars = math_min(strlen(text), max_size);
    u32 char_width = renderer_get_font_width(ui.config.fa_size);
    if (!hashtable_is_set(&ui.input_box_state, label)) {
        input_box_state_t state = {
            .text_offset = 0,
            .cursor_offset = num_chars,
            .content_flushed_last_frame = 0
        };
        hashtable_set(&ui.input_box_state, label, &state);
    }

    u32 label_width = renderer_get_font_width(ui.config.fa_size) * strlen(label);
    u32 label_height = renderer_get_font_height(ui.config.fa_size);
    u32 label_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 box_pos = ui_layout_get_position(win);
    vec2 box_size = vec2_create(
        win->size.x - ui.config.win_inner_pad * 2.0f - label_width - ui.config.input_box_label_x_pad,
        label_height + ui.config.input_box_y_pad * 2.0f
    );

    vec2 widget_size = vec2_create(
        box_size.x + label_width + ui.config.input_box_label_x_pad,
        box_size.y
    );

    ui_layout_add_widget(win, widget_size);

    vec2 box_screen_pos = vec2_create(
        win->position.x + box_pos.x,
        win->position.y + box_pos.y
    );

    if (rect_contains(box_screen_pos, box_size, ui.mouse_screen_pos)) {
        if (ui.mouse_left_pressed && !ui.mouse_left_pressed_last_frame) {
            ui.active_id = id;
            ui.input_active = true;
        } else {
            ui.hot_id = id;
        }
    } else {
        ui.hot_id = UI_INVALID_ID;
        if (!ui.mouse_left_pressed && ui.mouse_left_pressed_last_frame && ui.active_id == id) {
            ui.active_id = UI_INVALID_ID;
            ui.input_active = false;
        }
    }

    vec3 box_color = INPUT_BOX_BACKGROUND_COLOR;
    if (ui.hot_id == id && ui.active_id != id) {
        box_color = INPUT_BOX_HOVER_COLOR;
    }

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 box_render_pos = vec2_create(
        win_render_pos.x + box_pos.x + box_size.x / 2.0f,
        win_render_pos.y - box_pos.y - box_size.y / 2.0f
    );

    vec2 label_render_pos = vec2_create(
        box_render_pos.x + box_size.x / 2.0f + ui.config.input_box_label_x_pad,
        box_render_pos.y - label_bearing_y / 2.0f
    );
    
    renderer_draw_quad_color(box_render_pos, box_size, 0.0f, box_color, win->alpha);
    renderer_draw_text(label, ui.config.fa_size, label_render_pos, 1.0f, TEXT_COLOR, win->alpha);

    max_size--; // Leave the last byte for null-terminator

    input_box_state_t state;
    hashtable_get(&ui.input_box_state, label, &state);

    if (state.cursor_offset > num_chars) {
        state.text_offset = 0;
        state.cursor_offset = num_chars;
        hashtable_set(&ui.input_box_state, label, &state);
    }

    if (state.content_flushed_last_frame) {
        mem_zero(text, max_size);
        num_chars = 0;
        state.content_flushed_last_frame = false;
        hashtable_set(&ui.input_box_state, label, &state);
    }

    u32 num_chars_visible = (box_size.x - ui.config.input_box_text_x_pad * 2.0f) / char_width;

    if (ui.active_id == id) {
        if (!ui.input_active) {
            ui.active_id = UI_INVALID_ID;
        }

        u32 processed_keys = 0;
        while (!ring_buffer_is_empty(input_buffer) && processed_keys < INPUT_BUFFER_MAX_PROCESSED_KEYS) {
            u32 c;
            b8 status;
            ring_buffer_dequeue(input_buffer, &c, &status);
            if (status) {
                if (c == KEYCODE_Enter) {
                    state.text_offset = 0;
                    state.cursor_offset = 0;
                    state.content_flushed_last_frame = true;
                    hashtable_set(&ui.input_box_state, label, &state);
                    return true;
                } else if (c == KEYCODE_Backspace) {
                    if (num_chars > 0 && state.cursor_offset > 0) {
                        if (state.text_offset > 0) {
                            state.text_offset--;
                        }
                        if (state.cursor_offset < num_chars) {
                            mem_copy(text + state.cursor_offset - 1, text + state.cursor_offset, num_chars - state.cursor_offset);
                        }
                        text[num_chars-1] = '\0';

                        state.cursor_offset--;
                        num_chars--;

                        hashtable_set(&ui.input_box_state, label, &state);
                    }
                } else if (c == KEYCODE_Left) {
                    if (state.cursor_offset > 0) {
                        if (state.cursor_offset <= state.text_offset) {
                            state.text_offset--;
                        }
                        state.cursor_offset--;
                        hashtable_set(&ui.input_box_state, label, &state);
                    }
                } else if (c == KEYCODE_Right) {
                    if (state.cursor_offset < num_chars) {
                        state.cursor_offset++;
                        if (state.cursor_offset > state.text_offset + num_chars_visible) {
                            state.text_offset++;
                        }
                        hashtable_set(&ui.input_box_state, label, &state);
                    }
                } else {
                    // Insert character
                    if (num_chars >= num_chars_visible && state.cursor_offset == num_chars) {
                        state.text_offset++;
                    }
                    if (state.cursor_offset == num_chars) {
                        text[num_chars] = (char)c;
                    } else {
                        mem_copy(text + state.cursor_offset + 1, text + state.cursor_offset, num_chars - state.cursor_offset);
                        mem_copy(text + state.cursor_offset, &c, 1);
                    }

                    if (state.text_offset + num_chars_visible == state.cursor_offset) {
                        state.text_offset++;
                    }

                    num_chars++;
                    state.cursor_offset++;

                    hashtable_set(&ui.input_box_state, label, &state);
                }
                processed_keys++;
            }
        }

        ui.input_has_space = num_chars < max_size;

        // Draw border around input box
        renderer_draw_rect(box_render_pos, box_size, INPUT_BOX_BORDER_COLOR, 1.0f);

        // Draw cursor
        vec2 cursor_render_pos = vec2_create(
            box_render_pos.x - box_size.x / 2.0f + ui.config.input_box_text_x_pad + ((state.cursor_offset - state.text_offset) * char_width),
            box_render_pos.y
        );
        vec2 cursor_size = vec2_create(
            ui.config.input_box_cursor_width,
            box_size.y - ui.config.input_box_cursor_y_pad * 2.0f
        );
        renderer_draw_quad_color(cursor_render_pos, cursor_size, 0.0f, INPUT_BOX_CURSOR_COLOR, win->alpha);
    }

    if (num_chars > 0) {
        // Draw characters from text buffer on the input box
        vec2 chars_render_pos = vec2_create(
            box_render_pos.x - box_size.x / 2.0f + ui.config.input_box_text_x_pad,
            box_render_pos.y - label_bearing_y / 2.0f
        );

        char *buffer = text;
        if (num_chars > num_chars_visible) {
            buffer = arena_allocator_allocate(&frame_allocator, num_chars_visible);
            mem_copy(buffer, text + state.text_offset, num_chars_visible);
        }
        renderer_draw_text(buffer, ui.config.fa_size, chars_render_pos, 1.0f, TEXT_COLOR, win->alpha);
    }

    return false;
}

void ui_opt_carousel(const char *label, const char **items, u32 num_items, u32 *selected_item_index)
{
    ui_text(label);

    if (ui_button("<")) {
        *selected_item_index = ((*selected_item_index) + num_items - 1) % num_items;
    }
    ui_same_line();

    {
        ui_window_t *win = &ui.windows[ui.window_current_idx];

        u32 widest_item_num_chars = 0;
        for (u32 i = 0; i < num_items; i++) {
            u32 num_chars = strlen(items[i]);
            if (num_chars > widest_item_num_chars) {
                widest_item_num_chars = num_chars;
            }
        }

        u32 font_width = renderer_get_font_width(ui.config.fa_size);
        u32 font_height = renderer_get_font_height(ui.config.fa_size);
        u32 font_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

        const char *item = items[*selected_item_index];
        u32 item_width = font_width * strlen(item);

        f32 btn_height = font_height + ui.config.btn_pad * 2.0f;

        vec2 item_pos = ui_layout_get_position(win);
        vec2 widest_item_size = vec2_create(
            widest_item_num_chars * font_width,
            font_height
        );

        ui_layout_add_widget(win, widest_item_size);

        vec2 win_render_pos = vec2_create(
            win->position.x - main_window_size.x / 2.0f,
            main_window_size.y / 2.0f - win->position.y
        );

        vec2 item_render_pos = vec2_create(
            win_render_pos.x + item_pos.x + widest_item_size.x / 2.0f - item_width / 2.0f,
            win_render_pos.y - item_pos.y - btn_height / 2.0f - font_bearing_y / 2.0f
        );

        renderer_draw_text(item, ui.config.fa_size, item_render_pos, 1.0f, TEXT_COLOR, win->alpha);
    }

    ui_same_line();
    if (ui_button(">")) {
        *selected_item_index = ((*selected_item_index) + 1) % num_items;
    }
}

void ui_separator(void)
{
    ui_window_t *win = &ui.windows[ui.window_current_idx];

    win->layout.in_line = false;
    vec2 line_pos = ui_layout_get_position(win);
    vec2 line_size = vec2_create(
        win->size.x - ui.config.win_inner_pad * 2.0f,
        ui.config.separator_height
    );

    vec2 widget_size = vec2_create(
        line_size.x + ui.config.win_inner_pad * 2.0f,
        line_size.y
    );

    ui_layout_add_widget(win, widget_size);

    vec2 win_render_pos = vec2_create(
        win->position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - win->position.y
    );

    vec2 line_render_p1 = vec2_create(win_render_pos.x + line_pos.x,
                                      win_render_pos.y - line_pos.y - line_size.y / 2.0f);
    vec2 line_render_p2 = vec2_create(win_render_pos.x + line_pos.x + line_size.x,
                                      win_render_pos.y - line_pos.y - line_size.y / 2.0f);

    renderer_draw_line(line_render_p1, line_render_p2, TEXT_COLOR, win->alpha);
}

void ui_same_line(void)
{
    ui_window_t *win = &ui.windows[ui.window_current_idx];
    win->layout.in_line = true;
}

b8 ui_mouse_button_pressed_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];

    vec2 mp = input_get_mouse_position();
    i32 window_count = (i32)darray_length(ui.windows);
    for (i32 i = 0; i < window_count; i++) {
        ui_window_t *win = &ui.windows[i];

        if (rect_contains(win->position, win->size, mp)) {
            if (btn == MOUSEBUTTON_LEFT) {
                ui.mouse_left_pressed = true;
                ui.window_focused_idx = i;
                ui.window_captured_idx = i;
                return true;
            } else if (btn == MOUSEBUTTON_RIGHT) {
                ui.mouse_right_pressed = true;
                ui.window_focused_idx = i;
                ui.window_resizing_idx = i;
                return true;
            }
        } else if (ui.window_focused_idx != WINDOW_INVALID_IDX) {
            ui.window_focused_idx = WINDOW_INVALID_IDX;
        }
    }

    return false;
}

b8 ui_mouse_button_released_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];

    vec2 mp = input_get_mouse_position();

    i32 window_count = (i32)darray_length(ui.windows);
    for (i32 i = 0; i < window_count; i++) {
        ui_window_t *win = &ui.windows[i];

        if (rect_contains(win->position, win->size, mp)) {
            if (btn == MOUSEBUTTON_LEFT) {
                ui.mouse_left_pressed = false;
                ui.window_captured_idx = WINDOW_INVALID_IDX;
                return true;
            } else if (btn == MOUSEBUTTON_RIGHT) {
                ui.mouse_right_pressed = false;
                ui.window_resizing_idx = WINDOW_INVALID_IDX;
                return true;
            }
        } else {
            ui.window_resizing_idx = WINDOW_INVALID_IDX;
            ui.window_captured_idx = WINDOW_INVALID_IDX;
            if (btn == MOUSEBUTTON_LEFT) {
                ui.mouse_left_pressed = false;
            } else if (btn == MOUSEBUTTON_RIGHT) {
                ui.mouse_right_pressed = false;
            }
        }
    }

    return false;
}

b8 ui_mouse_moved_event_callback(event_code_e code, event_data_t data)
{
    f32 x = data.f32[0];
    f32 y = data.f32[1];
    f32 dx = x - ui.mouse_screen_pos.x;
    f32 dy = y - ui.mouse_screen_pos.y;

    if (ui.window_captured_idx != WINDOW_INVALID_IDX && ui.active_id == UI_INVALID_ID) {
        ui_window_t *win = &ui.windows[ui.window_captured_idx];
        if (win->draggable) {
            win->position.x += dx;
            win->position.y += dy;
        }
    } else if (ui.window_resizing_idx != WINDOW_INVALID_IDX) {
        ui_window_t *win = &ui.windows[ui.window_resizing_idx];
        if (win->resizable) {
            win->size.x += dx;
            win->size.y += dy;
            if (win->size.x <= MIN_WIN_WIDTH) {
                win->size.x = MIN_WIN_WIDTH;
            }
            if (win->size.y <= MIN_WIN_HEIGHT) {
                win->size.y = MIN_WIN_HEIGHT;
            }
        }
    }

    ui.mouse_screen_pos.x = x;
    ui.mouse_screen_pos.y = y;

    return ui.window_captured_idx != WINDOW_INVALID_IDX || ui.window_resizing_idx != WINDOW_INVALID_IDX;
}

b8 ui_mouse_scrolled_event_callback(event_code_e code, event_data_t data)
{
    vec2 mp = input_get_mouse_position();
    i32 window_count = (i32)darray_length(ui.windows);
    for (i32 i = 0; i < window_count; i++) {
        ui_window_t *win = &ui.windows[i];

        if (rect_contains(win->position, win->size, mp)) {
            // TODO: Add mouse scroll handling
            return true;
        }
    }

    return false;
}

b8 ui_char_pressed_event_callback(event_code_e code, event_data_t data)
{
    if (ui.input_active && ui.input_has_space) {
        b8 status;
        ring_buffer_enqueue(input_buffer, data.u32[0], &status);
        if (!status) {
            LOG_WARN("failed to enqueue pressed char into input buffer");
        }
        return true;
    }
    return false;
}

b8 ui_key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (ui.input_active) {
        if (key == KEYCODE_Backspace || key == KEYCODE_Left || key == KEYCODE_Right || key == KEYCODE_Enter) {
            b8 status;
            ring_buffer_enqueue(input_buffer, (u32)data.u16[0], &status);
            if (!status) {
                LOG_WARN("failed to enqueue pressed key into input buffer");
            }
        } else if (key == KEYCODE_Escape) {
            ui.input_active = false;
        }
        return true;
    }

    return false;
}

b8 ui_key_repeated_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (ui.input_active) {
        if (key == KEYCODE_Backspace || key == KEYCODE_Left || key == KEYCODE_Right) {
            b8 status;
            ring_buffer_enqueue(input_buffer, (u32)data.u16[0], &status);
            if (!status) {
                LOG_WARN("failed to enqueue repeated key into input buffer");
            }
        }
        return true;
    }

    return false;
}

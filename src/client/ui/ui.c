#include "ui.h"

#include <string.h>

#include "input.h"
#include "renderer.h"
#include "color_palette.h"
#include "common/maths.h"

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

#define UI_INVALID_ID (-1)

#define MIN_WIN_WIDTH 200
#define MIN_WIN_HEIGHT 200

// TODO: to be defined here
extern camera_t ui_camera;

extern vec2 main_window_size;

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
} ui_config_t;

typedef struct {
    ui_id hot_id;
    ui_id active_id;

    b8   mouse_left_pressed;
    b8   mouse_left_pressed_last_frame;
    b8   mouse_right_pressed;
    vec2 mouse_screen_pos;

    vec2 window_position; // top-left
    vec2 window_size;
    b8   window_hovered;
    b8   window_focused;
    b8   window_captured;
    b8   window_resizing;

    ui_layout_t layout;
    ui_config_t config;
} ui_t;

static ui_t ui;

static vec2 ui_layout_get_position(void)
{
    if (!ui.layout.in_line) {
        ui.layout.position.x = ui.config.win_inner_pad;
    } else {
        ui.layout.position.y -= ui.layout.last_widget_size.y + ui.config.widget_y_pad;
        ui.layout.in_line = false;
    }
    return ui.layout.position;
}

static void ui_layout_add_widget(vec2 size)
{
    ui.layout.position.x += size.x + ui.config.widget_x_pad;
    ui.layout.position.y += size.y + ui.config.widget_y_pad;
    ui.layout.last_widget_size = size;
}

static b8 rect_contains(vec2 pos /* top-left */, vec2 size, vec2 p)
{
    return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y && p.y <= pos.y + size.y;
}

void ui_init(void)
{
    // init config
    ui.hot_id                     = UI_INVALID_ID;
    ui.active_id                  = UI_INVALID_ID;
    ui.window_position            = (vec2){.x=450.0f, .y=100.0f};
    ui.window_size                = (vec2){.x=400.0f,  .y=500.0f};
    ui.config.fa_size             = FA32;
    ui.config.win_inner_pad       = 3.0f;
    ui.config.win_title_y_pad     = 3.0f;
    ui.config.widget_x_pad        = 5.0f;
    ui.config.widget_y_pad        = 5.0f;
    ui.config.checkbox_x_pad      = 5.0f;
    ui.config.radiobutton_x_pad   = 5.0f;
    ui.config.btn_pad             = 3.0f;
    ui.config.win_close_btn_scale = 0.8f;
    ui.config.separator_height    = 10.0f;
    ui.config.separator_thickness = 2.0f;
}

void ui_begin(const char *name, b8 *visible)
{
    ui.layout.position = vec2_create(ui.config.win_inner_pad, ui.config.win_inner_pad);
    ui.layout.size     = vec2_zero();
    ui.layout.in_line  = false;
    ui.layout.last_widget_size = vec2_zero();

    renderer_begin_scene(&ui_camera);

    // render window background
    vec2 win_render_pos = vec2_create(
        (ui.window_position.x - main_window_size.x / 2.0f) + ui.window_size.x / 2.0f,
        (main_window_size.y / 2.0f - ui.window_position.y) - ui.window_size.y / 2.0f
    );
    renderer_draw_quad_color(win_render_pos, ui.window_size, 0.0f, BACKGROUND_COLOR, 1.0f);

    // render border if focused
    if (ui.window_focused) {
        renderer_draw_rect(win_render_pos, ui.window_size, BORDER_COLOR, 1.0f);
    }

    { // render window header
        // title
        u32 text_height = renderer_get_font_height(ui.config.fa_size);
        u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);
        vec2 title_render_pos = vec2_create(
            win_render_pos.x - ui.window_size.x / 2.0f + ui.layout.position.x,
            win_render_pos.y + ui.window_size.y / 2.0f - text_bearing_y - ui.layout.position.y
        );
        renderer_draw_text(name, ui.config.fa_size, title_render_pos, 1.0f, TEXT_COLOR, 1.0f);

        ui.layout.position.y += text_height + ui.config.win_title_y_pad;

        // close button
        vec2 close_btn_size = vec2_create(text_height * ui.config.win_close_btn_scale, text_height * ui.config.win_close_btn_scale);
        vec2 close_btn_render_pos = vec2_create(
            win_render_pos.x + ui.window_size.x / 2.0f - ui.config.win_inner_pad - close_btn_size.x / 2.0f,
            win_render_pos.y + ui.window_size.y / 2.0f - ui.config.win_inner_pad - close_btn_size.y / 2.0f
        );

        vec2 close_btn_screen_pos = vec2_create(
            close_btn_render_pos.x - close_btn_size.x / 2.0f + main_window_size.x / 2.0f,
            main_window_size.y / 2.0f - close_btn_render_pos.y - close_btn_size.y / 2.0f
        );

        if (rect_contains(close_btn_screen_pos, close_btn_size, ui.mouse_screen_pos)) {
            close_btn_size.x = text_height * (ui.config.win_close_btn_scale + 0.1f);
            close_btn_size.y = text_height * (ui.config.win_close_btn_scale + 0.1f);
            if (ui.mouse_left_pressed) {
                ui.window_captured = false;
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
        renderer_draw_line(tr, bl, TEXT_COLOR, 1.0f);
        renderer_draw_line(tl, br, TEXT_COLOR, 1.0f);
    }

    // Make window header separator as high as its thickness, so that there is no extra margin
    f32 old_height = ui.config.separator_height;
    ui.config.separator_height = ui.config.separator_thickness;
    ui_separator();
    ui.config.separator_height = old_height;
}

void ui_end(void)
{
    ui.mouse_left_pressed_last_frame = ui.mouse_left_pressed;
    renderer_end_scene();
}

void ui_text(const char *text)
{
    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 text_pos = ui_layout_get_position();

    vec2 text_size = vec2_create(text_width, text_height);
    ui_layout_add_widget(text_size);

    vec2 win_render_pos = vec2_create(
        ui.window_position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - ui.window_position.y
    );

    vec2 text_render_pos = vec2_create(
        win_render_pos.x + text_pos.x,
        win_render_pos.y - text_pos.y - text_bearing_y
    );
    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, 1.0f);
}

b8 ui_button(const char *text, ui_id id)
{
    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 btn_pos = ui_layout_get_position();
    vec2 btn_size = vec2_create(text_width + ui.config.btn_pad * 2.0f, text_height + ui.config.btn_pad * 2.0f);
    ui_layout_add_widget(btn_size);

    b8 clicked = false;
    vec3 btn_color = BUTTON_BACKGROUND_COLOR;

    vec2 btn_screen_pos = vec2_create(
        ui.window_position.x + btn_pos.x,
        ui.window_position.y + btn_pos.y
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
            if (ui.mouse_left_pressed) {
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
        ui.window_position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - ui.window_position.y
    );

    vec2 btn_render_pos = vec2_create(
        win_render_pos.x + btn_pos.x + btn_size.x / 2.0f,
        win_render_pos.y - btn_pos.y - btn_size.y / 2.0f
    );
    renderer_draw_quad_color(btn_render_pos, btn_size, 0.0f, btn_color, 1.0f);

    // render button text content
    vec2 btn_text_render_pos = vec2_create(
        btn_render_pos.x - text_width / 2.0f,
        btn_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_text(text, ui.config.fa_size, btn_text_render_pos, 1.0f, TEXT_COLOR, 1.0f);

    return clicked;
}

void ui_checkbox(const char *text, b8 *is_checked, ui_id id)
{
    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 box_pos = ui_layout_get_position();
    vec2 box_size = vec2_create(text_height, text_height);

    vec2 widget_size = vec2_create(
        box_size.x + text_width + ui.config.checkbox_x_pad,
        box_size.y
    );

    ui_layout_add_widget(widget_size);

    vec3 box_color = CHECKBOX_BACKGROUND_COLOR;

    vec2 box_screen_pos = vec2_create(
        ui.window_position.x + box_pos.x,
        ui.window_position.y + box_pos.y
    );

    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
            if (rect_contains(box_screen_pos, box_size, ui.mouse_screen_pos)) {
                *is_checked = !(*is_checked);
            }
        }
    } else {
        if (rect_contains(box_screen_pos, box_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed) {
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
        ui.window_position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - ui.window_position.y
    );

    vec2 box_render_pos = vec2_create(
        win_render_pos.x + box_pos.x + box_size.x / 2.0f,
        win_render_pos.y - box_pos.y - box_size.y / 2.0f
    );

    vec2 text_render_pos = vec2_create(
        box_render_pos.x + box_size.x / 2.0f + ui.config.checkbox_x_pad,
        box_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_quad_color(box_render_pos, box_size, 0.0f, box_color, 1.0f);
    renderer_draw_rect(box_render_pos, box_size, CHECKBOX_BORDER_COLOR, 1.0f);

    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, 1.0f);
}

void ui_radiobutton(const char *text, i32 *selected_id, i32 self_id, ui_id id)
{
    u32 text_width = renderer_get_font_width(ui.config.fa_size) * strlen(text);
    u32 text_height = renderer_get_font_height(ui.config.fa_size);
    u32 text_bearing_y = renderer_get_font_bearing_y(ui.config.fa_size);

    vec2 btn_pos = ui_layout_get_position();
    vec2 btn_size = vec2_create(text_height, text_height);

    vec2 widget_size = vec2_create(
        btn_size.x + text_width + ui.config.radiobutton_x_pad,
        btn_size.y
    );

    ui_layout_add_widget(widget_size);

    vec3 btn_color = RADIO_BUTTON_BACKGROUND_COLOR;

    vec2 btn_screen_pos = vec2_create(
        ui.window_position.x + btn_pos.x,
        ui.window_position.y + btn_pos.y
    );

    if (ui.active_id == id) {
        if (!ui.mouse_left_pressed) {
            ui.active_id = UI_INVALID_ID;
            if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
                *selected_id = self_id;
            }
        }
    } else {
        if (rect_contains(btn_screen_pos, btn_size, ui.mouse_screen_pos)) {
            if (ui.mouse_left_pressed) {
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
        ui.window_position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - ui.window_position.y
    );

    vec2 btn_render_pos = vec2_create(
        win_render_pos.x + btn_pos.x + btn_size.x / 2.0f,
        win_render_pos.y - btn_pos.y - btn_size.y / 2.0f
    );

    vec2 text_render_pos = vec2_create(
        btn_render_pos.x + btn_size.x / 2.0f + ui.config.radiobutton_x_pad,
        btn_render_pos.y - text_bearing_y / 2.0f
    );
    renderer_draw_circle(btn_render_pos, btn_size.x / 2.0f, btn_color, 1.0f);
    renderer_draw_circle_thick(btn_render_pos, btn_size.x / 2.0f, 0.2f, RADIO_BUTTON_BORDER_COLOR, 1.0f);

    renderer_draw_text(text, ui.config.fa_size, text_render_pos, 1.0f, TEXT_COLOR, 1.0f);
}

void ui_separator(void)
{
    ui.layout.in_line = false;
    vec2 line_pos = ui_layout_get_position();
    vec2 line_size = vec2_create(
        ui.window_size.x - ui.config.win_inner_pad * 2.0f,
        ui.config.separator_height
    );

    vec2 widget_size = vec2_create(
        line_size.x + ui.config.win_inner_pad * 2.0f,
        line_size.y
    );

    ui_layout_add_widget(widget_size);

    vec2 win_render_pos = vec2_create(
        ui.window_position.x - main_window_size.x / 2.0f,
        main_window_size.y / 2.0f - ui.window_position.y
    );

    vec2 line_render_p1 = vec2_create(win_render_pos.x + line_pos.x,
                                      win_render_pos.y - line_pos.y - line_size.y / 2.0f);
    vec2 line_render_p2 = vec2_create(win_render_pos.x + line_pos.x + line_size.x,
                                      win_render_pos.y - line_pos.y - line_size.y / 2.0f);

    renderer_draw_line(line_render_p1, line_render_p2, TEXT_COLOR, 1.0f);
}

void ui_same_line(void)
{
    ui.layout.in_line = true;
}

b8 ui_mouse_button_pressed_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];

    vec2 mp = input_get_mouse_position();
    if (rect_contains(ui.window_position, ui.window_size, mp)) {
        if (btn == MOUSEBUTTON_LEFT) {
            ui.mouse_left_pressed = true;
            ui.window_focused = true;
            ui.window_captured = true;
            return true;
        } else if (btn == MOUSEBUTTON_RIGHT) {
            ui.mouse_right_pressed = true;
            ui.window_focused = true;
            ui.window_resizing = true;
            return true;
        }
    } else if (ui.window_focused) {
        ui.window_focused = false;
    }

    return false;
}

b8 ui_mouse_button_released_event_callback(event_code_e code, event_data_t data)
{
    u8 btn = data.u8[0];

    vec2 mp = input_get_mouse_position();
    if (rect_contains(ui.window_position, ui.window_size, mp)) {
        if (btn == MOUSEBUTTON_LEFT) {
            ui.mouse_left_pressed = false;
            ui.window_captured = false;
            return true;
        } else if (btn == MOUSEBUTTON_RIGHT) {
            ui.mouse_right_pressed = false;
            ui.window_resizing = false;
            return true;
        }
    } else {
        ui.window_resizing = false;
        ui.window_captured = false;
        ui.active_id = UI_INVALID_ID;
    }

    return false;
}

b8 ui_mouse_moved_event_callback(event_code_e code, event_data_t data)
{
    f32 x = data.f32[0];
    f32 y = data.f32[1];
    f32 dx = x - ui.mouse_screen_pos.x;
    f32 dy = y - ui.mouse_screen_pos.y;

    if (ui.window_captured && ui.active_id == UI_INVALID_ID) {
        ui.window_position.x += dx;
        ui.window_position.y += dy;
    } else if (ui.window_resizing) {
        ui.window_size.x += dx;
        ui.window_size.y += dy;
        if (ui.window_size.x <= MIN_WIN_WIDTH) {
            ui.window_size.x = MIN_WIN_WIDTH;
        }
        if (ui.window_size.y <= MIN_WIN_HEIGHT) {
            ui.window_size.y = MIN_WIN_HEIGHT;
        }
    }

    ui.mouse_screen_pos.x = x;
    ui.mouse_screen_pos.y = y;

    return ui.window_captured || ui.window_resizing;
}

b8 ui_mouse_scrolled_event_callback(event_code_e code, event_data_t data)
{
    vec2 mp = input_get_mouse_position();
    if (rect_contains(ui.window_position, ui.window_size, mp)) {
        // TODO: Add mouse scroll handling
        return true;
    }

    return false;
}

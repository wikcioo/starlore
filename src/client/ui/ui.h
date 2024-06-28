#pragma once

#include "defines.h"
#include "event.h"
#include "renderer.h"

typedef struct {
    vec2 position;
    vec2 size; // TODO: Implement window dynamic size
    b8   draggable : 1;
    b8   resizable : 1;
    font_atlas_size_e font_size;
} ui_window_config_t;

void ui_init         (void);
void ui_shutdown     (void);

void ui_begin_frame  (void);
void ui_end_frame    (void);
void ui_begin        (const char *name, b8 *visible);
void ui_begin_conf   (const char *name, ui_window_config_t *config, b8 *visible);
void ui_end          (void);

void ui_text         (const char *text);
b8   ui_button       (const char *text);
b8   ui_checkbox     (const char *text, b8 *is_checked);
b8   ui_radiobutton  (const char *text, i32 *selected_id, i32 self_id);
void ui_slider_float (const char *text, f32 *value, f32 low, f32 high);
b8   ui_input_text   (const char *label, char *text, u32 max_size);
void ui_opt_carousel (const char *label, const char **items, u32 num_items, u32 *selected_item_index);

void ui_separator    (void);
void ui_same_line    (void);

b8 ui_mouse_button_pressed_event_callback   (event_code_e code, event_data_t data);
b8 ui_mouse_button_released_event_callback  (event_code_e code, event_data_t data);
b8 ui_mouse_moved_event_callback            (event_code_e code, event_data_t data);
b8 ui_mouse_scrolled_event_callback         (event_code_e code, event_data_t data);
b8 ui_char_pressed_event_callback           (event_code_e code, event_data_t data);
b8 ui_key_pressed_event_callback            (event_code_e code, event_data_t data);
b8 ui_key_repeated_event_callback           (event_code_e code, event_data_t data);

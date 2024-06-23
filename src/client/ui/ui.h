#pragma once

#include "defines.h"
#include "event.h"

typedef i32 ui_id;

void ui_init        (void);
void ui_begin       (const char *name, b8 *visible);
void ui_end         (void);
void ui_text        (const char *text);
b8   ui_button      (const char *text, ui_id id);
void ui_checkbox    (const char *text, b8 *is_checked, ui_id id);
void ui_radiobutton (const char *text, i32 *selected_id, i32 self_id, ui_id id);
void ui_separator   (void);
void ui_same_line   (void);

b8 ui_mouse_button_pressed_event_callback   (event_code_e code, event_data_t data);
b8 ui_mouse_button_released_event_callback  (event_code_e code, event_data_t data);
b8 ui_mouse_moved_event_callback            (event_code_e code, event_data_t data);
b8 ui_mouse_scrolled_event_callback         (event_code_e code, event_data_t data);

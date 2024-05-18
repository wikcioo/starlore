#pragma once

#include "defines.h"
#include "common/maths.h"

b8 window_create(u32 width, u32 height, const char *title);
void window_destroy(void);

void window_poll_events(void);
void window_swap_buffers(void);

b8 window_is_cursor_captured(void);
void window_set_cursor_state(b8 captured);

vec2 window_get_size(void);
void *window_get_native_window(void);

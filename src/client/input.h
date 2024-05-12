#pragma once

#include "defines.h"
#include "common/maths.h"
#include "common/input_codes.h"

b8 input_is_key_pressed(keycode_e key);
b8 input_is_mouse_button_pressed(mousebutton_e button);
vec2 input_get_mouse_position(void);

#include "input.h"

#include <GLFW/glfw3.h>

#include "window.h"

b8 input_is_key_pressed(keycode_e key)
{
    i32 state = glfwGetKey(window_get_native_window(), key);
    return state == INPUTACTION_Press || state == INPUTACTION_Repeat;
}

b8 input_is_mouse_button_pressed(mousebutton_e button)
{
    i32 state = glfwGetMouseButton(window_get_native_window(), button);
    return state == INPUTACTION_Press;
}

vec2 input_get_mouse_position(void)
{
    f64 xpos, ypos;
    glfwGetCursorPos(window_get_native_window(), &xpos, &ypos);
    return vec2_create((f32)xpos, (f32)ypos);
}

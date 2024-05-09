#pragma once

typedef enum {
    // equivalent to glfw key codes
    KEYCODE_Space             = 32,
    KEYCODE_Apostrophe        = 39,  /* ' */
    KEYCODE_Comma             = 44,  /* , */
    KEYCODE_Minus             = 45,  /* - */
    KEYCODE_Period            = 46,  /* . */
    KEYCODE_Slash             = 47,  /* / */
    KEYCODE_D0                = 48,
    KEYCODE_D1                = 49,
    KEYCODE_D2                = 50,
    KEYCODE_D3                = 51,
    KEYCODE_D4                = 52,
    KEYCODE_D5                = 53,
    KEYCODE_D6                = 54,
    KEYCODE_D7                = 55,
    KEYCODE_D8                = 56,
    KEYCODE_D9                = 57,
    KEYCODE_Semicolon         = 59,  /* ; */
    KEYCODE_Equal             = 61,  /* = */
    KEYCODE_A                 = 65,
    KEYCODE_B                 = 66,
    KEYCODE_C                 = 67,
    KEYCODE_D                 = 68,
    KEYCODE_E                 = 69,
    KEYCODE_F                 = 70,
    KEYCODE_G                 = 71,
    KEYCODE_H                 = 72,
    KEYCODE_I                 = 73,
    KEYCODE_J                 = 74,
    KEYCODE_K                 = 75,
    KEYCODE_L                 = 76,
    KEYCODE_M                 = 77,
    KEYCODE_N                 = 78,
    KEYCODE_O                 = 79,
    KEYCODE_P                 = 80,
    KEYCODE_Q                 = 81,
    KEYCODE_R                 = 82,
    KEYCODE_S                 = 83,
    KEYCODE_T                 = 84,
    KEYCODE_U                 = 85,
    KEYCODE_V                 = 86,
    KEYCODE_W                 = 87,
    KEYCODE_X                 = 88,
    KEYCODE_Y                 = 89,
    KEYCODE_Z                 = 90,
    KEYCODE_LeftBracket       = 91,  /* [ */
    KEYCODE_Backslash         = 92,  /* \ */
    KEYCODE_RightBracket      = 93,  /* ] */
    KEYCODE_GraveAccent       = 96,  /* ` */
    KEYCODE_World1            = 161, /* non-US #1 */
    KEYCODE_World2            = 162, /* non-US #2 */

    KEYCODE_Escape            = 256,
    KEYCODE_Enter             = 257,
    KEYCODE_Tab               = 258,
    KEYCODE_Backspace         = 259,
    KEYCODE_Insert            = 260,
    KEYCODE_Delete            = 261,
    KEYCODE_Right             = 262,
    KEYCODE_Left              = 263,
    KEYCODE_Down              = 264,
    KEYCODE_Up                = 265,
    KEYCODE_PageUp            = 266,
    KEYCODE_PageDown          = 267,
    KEYCODE_Home              = 268,
    KEYCODE_End               = 269,
    KEYCODE_CapsLock          = 280,
    KEYCODE_ScrollLock        = 281,
    KEYCODE_NumLock           = 282,
    KEYCODE_PrintScreen       = 283,
    KEYCODE_Pause             = 284,
    KEYCODE_F1                = 290,
    KEYCODE_F2                = 291,
    KEYCODE_F3                = 292,
    KEYCODE_F4                = 293,
    KEYCODE_F5                = 294,
    KEYCODE_F6                = 295,
    KEYCODE_F7                = 296,
    KEYCODE_F8                = 297,
    KEYCODE_F9                = 298,
    KEYCODE_F10               = 299,
    KEYCODE_F11               = 300,
    KEYCODE_F12               = 301,
    KEYCODE_F13               = 302,
    KEYCODE_F14               = 303,
    KEYCODE_F15               = 304,
    KEYCODE_F16               = 305,
    KEYCODE_F17               = 306,
    KEYCODE_F18               = 307,
    KEYCODE_F19               = 308,
    KEYCODE_F20               = 309,
    KEYCODE_F21               = 310,
    KEYCODE_F22               = 311,
    KEYCODE_F23               = 312,
    KEYCODE_F24               = 313,
    KEYCODE_F25               = 314,
    KEYCODE_Kp0               = 320,
    KEYCODE_Kp1               = 321,
    KEYCODE_Kp2               = 322,
    KEYCODE_Kp3               = 323,
    KEYCODE_Kp4               = 324,
    KEYCODE_Kp5               = 325,
    KEYCODE_Kp6               = 326,
    KEYCODE_Kp7               = 327,
    KEYCODE_Kp8               = 328,
    KEYCODE_Kp9               = 329,
    KEYCODE_KpDecimal         = 330,
    KEYCODE_KpDivide          = 331,
    KEYCODE_KpMultiply        = 332,
    KEYCODE_KpSubtract        = 333,
    KEYCODE_KpAdd             = 334,
    KEYCODE_KpEnter           = 335,
    KEYCODE_KpEqual           = 336,
    KEYCODE_LeftShift         = 340,
    KEYCODE_LeftControl       = 341,
    KEYCODE_LeftAlt           = 342,
    KEYCODE_LeftSuper         = 343,
    KEYCODE_RightShift        = 344,
    KEYCODE_RightControl      = 345,
    KEYCODE_RightAlt          = 346,
    KEYCODE_RightSuper        = 347,
    KEYCODE_Menu              = 348,

    KEYCODE_Last              = KEYCODE_Menu
} keycode_e;

typedef enum {
    // equivalent to glfw action codes
    KEYACTION_Release = 0,
    KEYACTION_Press   = 1,
    KEYACTION_Repeat  = 2
} keyaction_e;

typedef enum {
    // equivalent to glfw mouse button codes
    MOUSEBUTTON_1        = 0,
    MOUSEBUTTON_2        = 1,
    MOUSEBUTTON_3        = 2,
    MOUSEBUTTON_4        = 3,
    MOUSEBUTTON_5        = 4,
    MOUSEBUTTON_6        = 5,
    MOUSEBUTTON_7        = 6,
    MOUSEBUTTON_8        = 7,
    MOUSEBUTTON_LAST     = MOUSEBUTTON_8,
    MOUSEBUTTON_LEFT     = MOUSEBUTTON_1,
    MOUSEBUTTON_RIGHT    = MOUSEBUTTON_2,
    MOUSEBUTTON_MIDDLE   = MOUSEBUTTON_3
} mousebutton_e;

#pragma once

#include "defines.h"

typedef enum {
    // invalid event code
    EVENT_CODE_NONE,

    // data usage:
    //   u16 key = data.u16[0]
    //   u16 mods = data.u16[1]
    EVENT_CODE_KEY_PRESSED,

    // data usage:
    //   u16 key = data.u16[0]
    //   u16 mods = data.u16[1]
    EVENT_CODE_KEY_RELEASED,

    // data usage:
    //   u16 key = data.u16[0]
    //   u16 mods = data.u16[1]
    EVENT_CODE_KEY_REPEATED,

    // data usage:
    //   u32 character = data.u32[0]
    EVENT_CODE_CHAR_PRESSED,

    // data usage:
    //   u8 btn = data.u8[0]
    EVENT_CODE_MOUSE_BUTTON_PRESSED,

    // data usage:
    //   u8 btn = data.u8[0]
    EVENT_CODE_MOUSE_BUTTON_RELEASED,

    // data usage:
    //   f32 x_pos = data.f32[0]
    //   f32 y_pos = data.f32[1]
    EVENT_CODE_MOUSE_MOVED,

    // data usage:
    //   f32 x_offset = data.f32[0]
    //   f32 y_offset = data.f32[1]
    EVENT_CODE_MOUSE_SCROLLED,

    EVENT_CODE_WINDOW_CLOSED,

    // data usage:
    //   u32 width = data.u32[0]
    //   u32 height = data.u32[0]
    EVENT_CODE_WINDOW_RESIZED,

    EVENT_CODE_WINDOW_MINIMIZED,

    EVENT_CODE_WINDOW_MAXIMIZED,

    EVENT_CODE_PLAYER_INIT,

    EVENT_CODE_GAME_WORLD_INIT,

    // data usage:
    //   u64 packet_memory_addr = data.u64[0]
    //   NOTE: receiver has to free the memory themselves
    EVENT_CODE_CHUNK_RECEIVED,

    EVENT_CODE_COUNT
} event_code_e;

typedef struct {
    // max 16 bytes of data in a single event
    union {
        u64 u64[2];
        i64 i64[2];
        f64 f64[2];

        u32 u32[4];
        i32 i32[4];
        f32 f32[4];

        u16 u16[8];
        i16 i16[8];

        u8 u8[16];
        i8 i8[16];
    };
} event_data_t;

typedef b8 (*fp_event_callback)(event_code_e code, event_data_t data);

b8 event_system_init(void);
void event_system_shutdown(void);

void event_system_register(event_code_e code, fp_event_callback callback);
void event_system_unregister(event_code_e code, fp_event_callback callback);
void event_system_fire(event_code_e code, event_data_t data);
void event_system_poll_events(void);

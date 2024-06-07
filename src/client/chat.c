#include "chat.h"

#include <stdio.h>
#include <string.h>

#include "input.h"
#include "window.h"
#include "renderer.h"
#include "color_palette.h"
#include "common/packet.h"
#include "common/input_codes.h"
#include "common/strings.h"
#include "common/asserts.h"
#include "common/logger.h"
#include "common/containers/darray.h"

#define MAX_INPUT_BUFFER_LENGTH (MESSAGE_MAX_CONTENT_LENGTH - 5)

typedef enum {
    CURSOR_DIR_LEFT,
    CURSOR_DIR_RIGHT
} cursor_dir_e;

extern vec2 main_window_size;

static f32 xoffset = 5.0f;
static f32 yoffset = 5.0f;
static const f32 width = 400.0f;
static const f32 height = 250.0f;
static const f32 gap = 5.0f;
static const f32 padding = 5.0f;

static b8 is_visible = true;
static b8 is_input_focused = false;

static u32 text_offset;
static u32 cursor_offset;

static u32 input_count;
static char input_buffer[MAX_INPUT_BUFFER_LENGTH];

extern char username[PLAYER_MAX_NAME_LENGTH];
extern i32 client_socket;

static font_atlas_size_e fa = FA16;
static u32 font_bearing_y;
static u32 font_height;
static u32 font_width;
static u32 num_chars_per_row;
static f32 input_box_height;
static f32 total_num_rows;

typedef struct {
    vec3 color;
    char data[PLAYER_MAX_NAME_LENGTH + MESSAGE_MAX_CONTENT_LENGTH];
} message_t;

static message_t *messages;

void chat_init(void)
{
    messages = darray_create(sizeof(message_t));
    font_bearing_y = renderer_get_font_bearing_y(fa);
    font_height = renderer_get_font_height(fa);
    font_width = renderer_get_font_width(fa);
    num_chars_per_row = (width - 2 * padding) / font_width;
    input_box_height = font_height + padding * 2;
    total_num_rows = height / font_height;

    f32 left   = -main_window_size.x / 2.0f;
    f32 bottom = -main_window_size.y / 2.0f;
    xoffset = left + 5.0f;
    yoffset = bottom + 5.0f;
}

void chat_shutdown(void)
{
    darray_destroy(messages);
}

static b8 insert_char(char c)
{
    if (input_count >= MAX_INPUT_BUFFER_LENGTH) {
        return false;
    }

    if (input_count >= num_chars_per_row && cursor_offset == input_count) {
        text_offset++;
    }

    if (cursor_offset == input_count) {
        input_buffer[input_count] = c;
    } else {
        memcpy(input_buffer + cursor_offset + 1, input_buffer + cursor_offset, input_count - cursor_offset);
        memcpy(input_buffer + cursor_offset, &c, 1);
    }

    if (text_offset + num_chars_per_row == cursor_offset) {
        text_offset++;
    }
    input_count++;
    cursor_offset++;

    return true;
}

static b8 remove_char(void)
{
    if (cursor_offset < 1 || input_count < 1) {
        return false;
    }

    if (text_offset > 0) {
        text_offset--;
    }

    if (cursor_offset == input_count) {
        input_buffer[input_count - 1] = '\0';
    } else {
        memcpy(input_buffer + cursor_offset - 1, input_buffer + cursor_offset, input_count - cursor_offset);
        input_buffer[input_count - 1] = '\0';
    }

    cursor_offset--;
    input_count--;

    return true;
}

static void move_cursor(cursor_dir_e dir)
{
    if (dir == CURSOR_DIR_LEFT && cursor_offset > 0) {
        if (cursor_offset <= text_offset) {
            text_offset--;
        }
        cursor_offset--;
    } else if (dir == CURSOR_DIR_RIGHT) {
        if (cursor_offset < input_count) {
            cursor_offset++;
            if (cursor_offset > text_offset + num_chars_per_row) {
                text_offset++;
            }
        }
    }
}

b8 chat_key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    u16 mods = data.u16[1];

    if (key == KEYCODE_T && !is_input_focused) {
        is_visible = !is_visible;
        return true;
    }

    if (is_visible && key == KEYCODE_L && mods & KEYMOD_CONTROL) {
        is_input_focused = true;
        return true;
    }

    if (!is_input_focused) {
        return false;
    }

    if (key == KEYCODE_Enter) {
        b8 parsed_successfully = false;
        if (input_buffer[0] == '/') {
            if (strncmp(&input_buffer[1], "whoami", strlen("whoami")) == 0) {
                chat_add_system_message(username);
                parsed_successfully = true;
            }
        } else {
            char *input_trimmed = string_trim(input_buffer);
            if (strlen(input_trimmed) > 0) {
                u32 input_trimmed_length = strlen(input_trimmed);

                packet_message_t message_packet = {0};
                message_packet.type = MESSAGE_TYPE_PLAYER;
                u32 username_size = strlen(username) > PLAYER_MAX_NAME_LENGTH ? PLAYER_MAX_NAME_LENGTH : strlen(username);
                strncpy(message_packet.author, username, username_size);
                u32 content_size = input_trimmed_length > MAX_INPUT_BUFFER_LENGTH ? MAX_INPUT_BUFFER_LENGTH : input_trimmed_length;
                strncpy(message_packet.content, input_trimmed, content_size);

                if (!packet_send(client_socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                    LOG_ERROR("failed to send message packet");
                }
                parsed_successfully = true;
            }
        }

        if (parsed_successfully) {
            memset(input_buffer, 0, input_count);
            input_count = 0;
            cursor_offset = 0;
            text_offset = 0;
        }
    } else if (key == KEYCODE_Escape) {
        is_input_focused = false;
    } else if (key == KEYCODE_Backspace) {
        remove_char();
    } else if (key == KEYCODE_Left) {
        move_cursor(CURSOR_DIR_LEFT);
    } else if (key == KEYCODE_Right) {
        move_cursor(CURSOR_DIR_RIGHT);
    } else if (key >= KEYCODE_Space && key <= KEYCODE_GraveAccent) {
    }

    return true;
}

b8 chat_key_repeated_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (key == KEYCODE_Backspace && is_visible && is_input_focused) {
        remove_char();
        return true;
    } else if (key == KEYCODE_Left) {
        move_cursor(CURSOR_DIR_LEFT);
        return true;
    } else if (key == KEYCODE_Right) {
        move_cursor(CURSOR_DIR_RIGHT);
        return true;
    }

    return false;
}

b8 chat_char_pressed_event_callback(event_code_e code, event_data_t data)
{
    if (!is_input_focused) {
        return false;
    }

    insert_char(data.u32[0]);
    return true;
}

b8 chat_mouse_button_pressed_event_callback(event_code_e code, event_data_t data)
{
    if (!is_visible) {
        return false;
    }

    if (data.u8[0] == MOUSEBUTTON_LEFT) {
        vec2 mouse_pos = input_get_mouse_position();
        mouse_pos.x =   mouse_pos.x - main_window_size.x / 2.0f;
        mouse_pos.y = -(mouse_pos.y - main_window_size.y / 2.0f);

        if (xoffset <= mouse_pos.x && mouse_pos.x <= xoffset + width &&
            yoffset <= mouse_pos.y && mouse_pos.y <= yoffset + input_box_height) {
            is_input_focused = true;
            return true;
        } else {
            is_input_focused = false;
            return false;
        }
    }

    return false;
}

b8 chat_window_resized_event_callback(event_code_e code, event_data_t data)
{
    u32 w = data.u32[0];
    u32 h = data.u32[1];
    f32 left = -(w / 2.0f);
    f32 bottom = -(h / 2.0f);
    xoffset = left + 5.0f;
    yoffset = bottom + 5.0f;
    return false;
}

void chat_add_player_message(chat_player_message_t message)
{
    if (strcmp(username, message.name) == 0) {
        memcpy(message.name, "you", 4);
    }

    message_t msg = {0};
    msg.color = COLOR_MILK;
    snprintf(msg.data, sizeof(msg.data), "%s: %s", message.name, message.content);
    darray_push(messages, msg);
}

void chat_add_system_message(const char *message)
{
    message_t msg = {0};
    msg.color = COLOR_GOLDEN_YELLOW;
    snprintf(msg.data, sizeof(msg.data), "%s", message);
    darray_push(messages, msg);
}

static void chat_render_next_row(const char *str, u32 length, u32 offset, vec3 color)
{
    vec2 pos = vec2_create(
        xoffset + padding,
        yoffset + input_box_height + gap + padding + (offset * font_height)
    );

    char buffer[128] = {0};
    ASSERT(length < 128);
    memcpy(buffer, str, length);
    renderer_draw_text(buffer, fa, pos, 1.0f, color, 1.0f);
}

// NOTE: Works for monospaced font only
void chat_render(void)
{
    if (!is_visible) {
        return;
    }

    vec2 input_box_position = vec2_create(xoffset + width/2, yoffset + input_box_height/2);
    vec2 input_box_size = vec2_create(width, input_box_height);

    // Draw input box background
    renderer_draw_quad_color(input_box_position, input_box_size, 0.0f, COLOR_BLACK, 0.6f);

    if (is_input_focused) {
        // Draw border around input box
        renderer_draw_rect(input_box_position, input_box_size, COLOR_GOLDEN_YELLOW, 1.0f);
    }

    if (input_count > 0) {
        // Draw characters from input buffer on the input box
        vec2 chars_pos = vec2_create(
            xoffset + padding,
            math_round(yoffset + input_box_height/2.0f - font_bearing_y/2.0f)
        );
        if (input_count > num_chars_per_row) {
            char buffer[MAX_INPUT_BUFFER_LENGTH] = {0};
            memcpy(buffer, input_buffer + text_offset, num_chars_per_row);
            renderer_draw_text(buffer, fa, chars_pos, 1.0f, COLOR_MILK, 1.0f);
        } else {
            renderer_draw_text(input_buffer, fa, chars_pos, 1.0f, COLOR_MILK, 1.0f);
        }
    }

    // Draw cursor if input box focused
    if (is_input_focused) {
        vec2 cursor_position = vec2_create(
            xoffset + padding + ((cursor_offset - text_offset) * font_width) + 1.0f,
            yoffset + input_box_height/2
        );
        vec2 cursor_size = vec2_create(
            1.0f,
            font_height
        );
        renderer_draw_quad_color(cursor_position, cursor_size, 0.0f, COLOR_MILK, 1.0f);
    }

    // Draw messages box background
    renderer_draw_quad_color(vec2_create(xoffset + width/2, yoffset + input_box_height + gap + height/2),
                             vec2_create(width, height),
                             0.0f, COLOR_BLACK, 0.6f);

    // Draw messages
    u64 messages_length = darray_length(messages);
    for (u64 i = 0, total_rows_parsed = 0; i < messages_length; i++, total_rows_parsed++) {
        if ((total_rows_parsed+1) * font_height > height) {
            // If the next row ends up outside the top side of the chat box, then exit
            break;
        }

        message_t *message = &messages[messages_length-i-1];

        u32 message_length = strlen(message->data);
        u32 message_width_px = message_length * font_width;
        u32 message_additional_occupied_rows = message_width_px / (width - 2 * padding + 1);

        if (message_additional_occupied_rows > 0) {
            // Display original message as multiple sub messages with each row having maximum of num_chars_per_row letters
            // Go top to bottom, so ignore the first sub messages which do not fit in the designated area
            i32 row_idx;
            for (row_idx = message_additional_occupied_rows; row_idx >= 0; row_idx--) {
                if (total_rows_parsed + row_idx + 1 > total_num_rows) {
                    // Do not render sub messages which do not fit in the chat box area
                    continue;
                }

                // Get the pointer to the next sub message and render num_chars_per_row at a time
                char *sub_message = message->data + ((message_additional_occupied_rows - row_idx) * num_chars_per_row);
                chat_render_next_row(sub_message, num_chars_per_row, total_rows_parsed + row_idx, message->color);
            }

            // Render the last remaining sub message, which might not be num_chars_per_row wide
            char *sub_message = message->data + ((message_additional_occupied_rows - row_idx) * num_chars_per_row);
            u32 last_sub_message_length = message_length - (message_additional_occupied_rows * num_chars_per_row);
            chat_render_next_row(sub_message, last_sub_message_length, total_rows_parsed, message->color);

            total_rows_parsed += message_additional_occupied_rows;
        } else {
            chat_render_next_row(message->data, message_length, total_rows_parsed, message->color);
        }
    }
}

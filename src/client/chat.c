#include "chat.h"

#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "color_palette.h"
#include "common/strings.h"
#include "common/asserts.h"
#include "common/logger.h"
#include "common/containers/darray.h"

#define MAX_MESSAGE_LENGTH (MAX_PLAYER_NAME_LENGTH + MAX_MESSAGE_CONTENT_LENGTH)

static const f32 xoffset = 5.0f;
static const f32 yoffset = 5.0f;
static const f32 width = 400.0f;
static const f32 height = 250.0f;
static const f32 gap = 5.0f;
static const f32 padding = 5.0f;

extern char username[MAX_PLAYER_NAME_LENGTH];

font_atlas_size_e fa = FA16;
u32 font_height;
u32 font_width;
u32 num_chars_per_row;
f32 input_box_height;
f32 total_num_rows;

typedef struct {
    vec3 color;
    char data[MAX_MESSAGE_LENGTH];
} message_t;

static message_t *messages;

void chat_init(void)
{
    messages = darray_create(sizeof(message_t));
    font_height = renderer_get_font_height(fa);
    font_width = renderer_get_font_width(fa);
    num_chars_per_row = (width - 2 * padding) / font_width;
    input_box_height = font_height + padding * 2;
    total_num_rows = height / font_height;
}

void chat_shutdown(void)
{
    darray_destroy(messages);
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
    // Draw input box background
    renderer_draw_quad(vec2_create(xoffset + width/2, yoffset + font_height/2 + padding),
                       vec2_create(width, input_box_height),
                       0.0f,
                       COLOR_BLACK, 0.3f);

    // Draw messages box background
    renderer_draw_quad(vec2_create(xoffset + width/2, yoffset + height/2 + input_box_height + gap),
                       vec2_create(width, height),
                       0.0f,
                       COLOR_BLACK, 0.3f);

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

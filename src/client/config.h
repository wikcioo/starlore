#pragma once

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define VSYNC_ENABLED 1

#define INPUT_BUFFER_SIZE    KiB(8)
#define OVERFLOW_BUFFER_SIZE KiB(4)

#define PLAYER_ANIMATION_FPS 10
#define PLAYER_KEYPRESS_RING_BUFFER_CAPACITY 256
#define PLAYER_DAMAGED_HIGHLIGHT_DURATION 0.2f

#define CAMERA_MOVE_SPEED_PPS 1200
#define CAMERA_MOVE_BORDER_OFFSET 5
#define CAMERA_ZOOM_MAX 5.0f
#define CAMERA_ZOOM_MIN 0.5f

#define TEX_COORD_COUNT 4

#define CHUNK_CACHE_MAX_ITEMS 48

#define LOG_TEXTURE_CREATE               0
#define LOG_REACH_CHUNK_CACHE_SIZE_LIMIT 0
#define LOG_CHUNK_TRANSACTIONS           0
#define LOG_NETWORK                      0

#pragma once

#include "defines.h"

typedef enum {
    IMAGE_FORMAT_NONE,
    IMAGE_FORMAT_R8,
    IMAGE_FORMAT_RGB8,
    IMAGE_FORMAT_RGBA8,
    IMAGE_FORMAT_COUNT
} image_format_e;

typedef struct {
    u32 width;
    u32 height;
    b8 generate_mipmaps;
    image_format_e format;
} texture_specification_t;

typedef struct {
    u32 id;
    u32 width;
    u32 height;
} texture_t;

void texture_create_from_path(const char *filepath, texture_t *out_texture);
void texture_create_from_spec(texture_specification_t spec, void *data, texture_t *out_texture);
void texture_destroy(texture_t *texture);

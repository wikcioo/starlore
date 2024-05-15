#include "texture.h"

#include <memory.h>

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "common/logger.h"
#include "common/asserts.h"
#include "common/filesystem.h"

static u32 image_format_to_opengl_format(image_format_e format)
{
    ASSERT(format > IMAGE_FORMAT_NONE && format < IMAGE_FORMAT_COUNT);

    switch (format) {
        case IMAGE_FORMAT_R8:    return GL_RED;
        case IMAGE_FORMAT_RGB8:  return GL_RGB;
        case IMAGE_FORMAT_RGBA8: return GL_RGBA;
        default:
            LOG_WARN("unknown image format");
            return 0;
    }
}

void texture_create_from_path(const char *filepath, texture_t *out_texture)
{
    ASSERT(filepath);
    ASSERT(out_texture);
    ASSERT(filesystem_exists(filepath));

    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8 *image_data = stbi_load(filepath, &width, &height, &channels, STBI_default);
    if (image_data == NULL) {
        LOG_ERROR("failed to load image at %s", filepath);
        return;
    }

    memset(out_texture, 0, sizeof(texture_t));
    out_texture->width = width;
    out_texture->height = height;

    u32 format = 0;
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_RGB;
    } else if (channels == 4) {
        format = GL_RGBA;
    } else {
        LOG_WARN("unsupported number of channels (%d) for image at %s", channels, filepath);
        return;
    }

    glGenTextures(1, &out_texture->id);
    glBindTexture(GL_TEXTURE_2D, out_texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);

    stbi_image_free(image_data);
}

void texture_create_from_spec(texture_specification_t spec, void *data, texture_t *out_texture)
{
    ASSERT(out_texture);

    memset(out_texture, 0, sizeof(texture_t));

    u32 format = image_format_to_opengl_format(spec.format);

    glGenTextures(1, &out_texture->id);
    glBindTexture(GL_TEXTURE_2D, out_texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, spec.width, spec.height, 0, format, GL_UNSIGNED_BYTE, data);
}

void texture_destroy(texture_t *texture)
{
    ASSERT(texture);
    glDeleteTextures(1, &texture->id);
}

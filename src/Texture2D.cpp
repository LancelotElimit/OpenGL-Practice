#include "Texture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <limits>

bool Texture2D::loadMemory(const unsigned char* bytes, std::size_t size, bool flipVertically) {
    if (!bytes || size > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    int width = 0, height = 0, channels = 0;
    auto* pixels = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) return false;
    createRGBA(width, height, pixels);
    stbi_image_free(pixels);
    return true;
}

Texture2D::~Texture2D() {
    release();
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : texture_(other.texture_) {
    other.texture_ = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    texture_ = other.texture_;
    other.texture_ = 0;
    return *this;
}

bool Texture2D::loadRGBA(
    const std::filesystem::path& path,
    bool flipVertically
) {
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(
        path.string().c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels) {
        std::cerr << "Could not load texture: "
                  << path << "\n";
        return false;
    }

    createRGBA(width, height, pixels);
    stbi_image_free(pixels);
    return true;
}

void Texture2D::createRGBA(
    int width,
    int height,
    const unsigned char* pixels,
    GLint minFilter,
    GLint magFilter
) {
    release();
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );
}

void Texture2D::bind(GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

GLuint Texture2D::id() const {
    return texture_;
}

void Texture2D::destroy() {
    release();
}

void Texture2D::release() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}

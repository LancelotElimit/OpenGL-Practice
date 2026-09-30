#pragma once

#include <glad/gl.h>

#include <filesystem>
#include <cstddef>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    bool loadRGBA(const std::filesystem::path& path, bool flipVertically);
    bool loadMemory(const unsigned char* bytes, std::size_t size, bool flipVertically);
    void createRGBA(
        int width,
        int height,
        const unsigned char* pixels,
        GLint minFilter = GL_NEAREST,
        GLint magFilter = GL_NEAREST
    );

    void bind(GLuint textureUnit) const;
    GLuint id() const;
    void destroy();

private:
    void release();

    GLuint texture_ = 0;
};

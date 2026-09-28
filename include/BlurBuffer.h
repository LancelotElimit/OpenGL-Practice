#pragma once

#include <glad/gl.h>

// Two HDR color targets used alternately by separable blur passes.
class BlurBuffer {
public:
    BlurBuffer() = default;
    ~BlurBuffer();

    BlurBuffer(const BlurBuffer&) = delete;
    BlurBuffer& operator=(const BlurBuffer&) = delete;

    bool resize(GLsizei width, GLsizei height);
    void beginWrite(int index) const;
    void bindTexture(int index, GLuint textureUnit) const;
    void destroy();

private:
    bool create(GLsizei width, GLsizei height);

    GLuint framebuffers_[2] = {0, 0};
    GLuint textures_[2] = {0, 0};
    GLsizei width_ = 0;
    GLsizei height_ = 0;
};

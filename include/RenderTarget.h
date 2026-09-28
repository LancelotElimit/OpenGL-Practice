#pragma once

#include <glad/gl.h>

// An off-screen HDR framebuffer with color and depth/stencil storage.
class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool resize(GLsizei width, GLsizei height);
    void begin() const;
    void end() const;
    void bindColorTexture(GLuint textureUnit) const;
    void destroy();

private:
    bool create(GLsizei width, GLsizei height);

    GLuint framebuffer_ = 0;
    GLuint colorTexture_ = 0;
    GLuint depthStencilBuffer_ = 0;
    GLsizei width_ = 0;
    GLsizei height_ = 0;
};

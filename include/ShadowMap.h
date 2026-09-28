#pragma once

#include <glad/gl.h>

// Owns a depth-only 2D texture and its framebuffer.
class ShadowMap2D {
public:
    ShadowMap2D() = default;
    ~ShadowMap2D();

    ShadowMap2D(const ShadowMap2D&) = delete;
    ShadowMap2D& operator=(const ShadowMap2D&) = delete;

    bool create(GLsizei size);
    void beginWrite() const;
    void endWrite() const;
    void bind(GLuint textureUnit) const;
    void destroy();

private:
    GLuint framebuffer_ = 0;
    GLuint texture_ = 0;
    GLsizei size_ = 0;
};

// Owns a six-face depth cubemap and the framebuffer used to fill each face.
class ShadowCubeMap {
public:
    ShadowCubeMap() = default;
    ~ShadowCubeMap();

    ShadowCubeMap(const ShadowCubeMap&) = delete;
    ShadowCubeMap& operator=(const ShadowCubeMap&) = delete;

    bool create(GLsizei size);
    void beginFace(int face) const;
    void endWrite() const;
    void bind(GLuint textureUnit) const;
    void destroy();

private:
    GLuint framebuffer_ = 0;
    GLuint texture_ = 0;
    GLsizei size_ = 0;
};

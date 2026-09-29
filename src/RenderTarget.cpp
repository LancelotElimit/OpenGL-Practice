#include "RenderTarget.h"

#include <iostream>

RenderTarget::~RenderTarget() {
    destroy();
}

bool RenderTarget::resize(GLsizei width, GLsizei height) {
    if (framebuffer_ != 0 && width == width_ && height == height_) {
        return true;
    }

    destroy();
    return create(width, height);
}

bool RenderTarget::create(GLsizei width, GLsizei height) {
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA16F,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        colorTexture_,
        0
    );

    // The water pass samples the opaque scene, never the texture it writes.
    glGenTextures(1, &colorCopyTexture_);
    glBindTexture(GL_TEXTURE_2D, colorCopyTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &depthStencilBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer_);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,
        width_,
        height_
    );
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        depthStencilBuffer_
    );

    // Depth is copied after opaque rendering. Sampling the live depth
    // attachment while writing particles would create a feedback loop.
    glGenFramebuffers(1, &depthCopyFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, depthCopyFramebuffer_);
    glGenTextures(1, &depthCopyTexture_);
    glBindTexture(GL_TEXTURE_2D, depthCopyTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
                 width_, height_, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, depthCopyTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool depthComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE && depthComplete;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete) {
        std::cerr << "Scene render target is not complete.\n";
        destroy();
    }
    return complete;
}

void RenderTarget::begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, width_, height_);
}

void RenderTarget::end() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::bindColorTexture(GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
}

GLuint RenderTarget::colorTexture() const {
    return colorTexture_;
}

void RenderTarget::copyColorForSampling() const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorCopyTexture_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void RenderTarget::bindColorCopy(GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, colorCopyTexture_);
}

void RenderTarget::copyDepthForSampling() const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthCopyFramebuffer_);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void RenderTarget::bindDepthCopy(GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthCopyTexture_);
}

void RenderTarget::destroy() {
    if (colorCopyTexture_ != 0) {
        glDeleteTextures(1, &colorCopyTexture_);
        colorCopyTexture_ = 0;
    }
    if (depthCopyTexture_ != 0) {
        glDeleteTextures(1, &depthCopyTexture_);
        depthCopyTexture_ = 0;
    }
    if (depthCopyFramebuffer_ != 0) {
        glDeleteFramebuffers(1, &depthCopyFramebuffer_);
        depthCopyFramebuffer_ = 0;
    }
    if (depthStencilBuffer_ != 0) {
        glDeleteRenderbuffers(1, &depthStencilBuffer_);
        depthStencilBuffer_ = 0;
    }
    if (colorTexture_ != 0) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (framebuffer_ != 0) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

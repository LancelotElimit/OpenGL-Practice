#include "BlurBuffer.h"

#include <iostream>

BlurBuffer::~BlurBuffer() {
    destroy();
}

bool BlurBuffer::resize(GLsizei width, GLsizei height) {
    if (framebuffers_[0] != 0 && width == width_ && height == height_) {
        return true;
    }

    destroy();
    return create(width, height);
}

bool BlurBuffer::create(GLsizei width, GLsizei height) {
    width_ = width;
    height_ = height;

    glGenFramebuffers(2, framebuffers_);
    glGenTextures(2, textures_);
    for (int index = 0; index < 2; ++index) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[index]);
        glBindTexture(GL_TEXTURE_2D, textures_[index]);
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
            textures_[index],
            0
        );

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Blur framebuffer is not complete.\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy();
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void BlurBuffer::beginWrite(int index) const {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[index]);
    glViewport(0, 0, width_, height_);
}

void BlurBuffer::bindTexture(int index, GLuint textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textures_[index]);
}

void BlurBuffer::destroy() {
    glDeleteTextures(2, textures_);
    glDeleteFramebuffers(2, framebuffers_);
    textures_[0] = 0;
    textures_[1] = 0;
    framebuffers_[0] = 0;
    framebuffers_[1] = 0;
    width_ = 0;
    height_ = 0;
}

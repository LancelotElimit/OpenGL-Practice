#include "Fluid2D.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace {
constexpr float FixedStep = 1.0f / 60.0f;
}

Fluid2D::Fluid2D(const std::filesystem::path& shaderDirectory)
    : program_(shaderDirectory / "postprocess.vert",
               shaderDirectory / "fluid2d.frag", "2D smoke solver"),
      sceneProgram_(shaderDirectory/"smoke_scene.vert",shaderDirectory/"smoke_scene.frag","Smoke scene surface") {
    if (!program_.valid() || !sceneProgram_.valid()) return;
    const float quad[] = {
        -1, -1, 0, 0,  1, -1, 1, 0,  1, 1, 1, 1,
        -1, -1, 0, 0,  1, 1, 1, 1,  -1, 1, 0, 1
    };
    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    bool complete = true;
    for (Target& t : velocity_) complete &= createTarget(t, GL_RGBA16F);
    for (Target& t : density_) complete &= createTarget(t, GL_RGBA16F);
    for (Target& t : pressure_) complete &= createTarget(t, GL_RGBA16F);
    complete &= createTarget(divergence_, GL_RGBA16F);
    complete &= createTarget(display_, GL_RGBA8);
    glGenTextures(1, &obstacleTexture_);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Size, Size, 0, GL_RED,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    obstaclePixels_.resize(Size * Size);
    valid_ = complete;
    if (!complete) {
        std::cerr << "2D smoke framebuffer creation failed.\n";
        return;
    }
    program_.use();
    glUniform1i(program_.uniform("uVelocity"), 0);
    glUniform1i(program_.uniform("uSource"), 1);
    glUniform1i(program_.uniform("uPressure"), 2);
    glUniform1i(program_.uniform("uDivergence"), 3);
    glUniform1i(program_.uniform("uObstacle"), 4);
    resetObstacles(true);
    reset();
}

Fluid2D::~Fluid2D() { destroy(); }
bool Fluid2D::valid() const { return valid_; }
void Fluid2D::drawScene(const glm::mat4& viewProjection,const glm::mat4& model,float opacity) {
    if(!valid_) return;
    sceneProgram_.use();
    glUniformMatrix4fv(sceneProgram_.uniform("uViewProjection"),1,GL_FALSE,&viewProjection[0][0]);
    glUniformMatrix4fv(sceneProgram_.uniform("uModel"),1,GL_FALSE,&model[0][0]);
    glUniform1f(sceneProgram_.uniform("uOpacity"),opacity);
    glUniform1i(sceneProgram_.uniform("uDensity"),0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,density_[densityRead_].texture);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    glBindVertexArray(quadVao_); glDrawArrays(GL_TRIANGLES,0,6); glBindVertexArray(0);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}
GLuint Fluid2D::displayTexture() const { return display_.texture; }
int Fluid2D::resolution() const { return Size; }
float Fluid2D::updateMilliseconds() const { return updateMilliseconds_; }
int Fluid2D::passCount() const { return passCount_; }

bool Fluid2D::createTarget(Target& target, GLenum format) {
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, Size, Size, 0, GL_RGBA,
                 format == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, target.texture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void Fluid2D::clearTarget(const Target& target) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, Size, Size);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Fluid2D::reset() {
    if (!valid_) return;
    for (const Target& t : velocity_) clearTarget(t);
    for (const Target& t : density_) clearTarget(t);
    for (const Target& t : pressure_) clearTarget(t);
    clearTarget(divergence_);
    clearTarget(display_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    velocityRead_ = densityRead_ = pressureRead_ = 0;
    accumulator_ = sourceTime_ = 0.0f;
    pendingInjection_ = false;
    displayDirty_ = true;
}

void Fluid2D::resetObstacles(bool centerDisk) {
    if (!valid_) return;
    for (int y = 0; y < Size; ++y)
        for (int x = 0; x < Size; ++x) {
            const float u = (x + 0.5f) / Size;
            const float v = (y + 0.5f) / Size;
            const bool border = x < 2 || y < 2 || x >= Size - 2 || y >= Size - 2;
            const bool disk = centerDisk && (u - 0.56f) * (u - 0.56f)
                + (v - 0.47f) * (v - 0.47f) < 0.09f * 0.09f;
            obstaclePixels_[y * Size + x] = (border || disk) ? 255 : 0;
        }
    glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Size, Size,
                    GL_RED, GL_UNSIGNED_BYTE, obstaclePixels_.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    displayDirty_ = true;
}

void Fluid2D::paintObstacle(glm::vec2 uv, float radius) {
    if (!valid_) return;
    const int cx = static_cast<int>(uv.x * Size);
    const int cy = static_cast<int>(uv.y * Size);
    const int r = std::max(1, static_cast<int>(radius * Size));
    for (int y = std::max(0, cy-r); y <= std::min(Size-1, cy+r); ++y)
        for (int x = std::max(0, cx-r); x <= std::min(Size-1, cx+r); ++x)
            if ((x-cx)*(x-cx) + (y-cy)*(y-cy) <= r*r)
                obstaclePixels_[y * Size + x] = 255;
    glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Size, Size,
                    GL_RED, GL_UNSIGNED_BYTE, obstaclePixels_.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    displayDirty_ = true;
}

void Fluid2D::inject(glm::vec2 uv, glm::vec2 velocity, float strength) {
    pendingInjection_ = true;
    injectionUv_ = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
    injectionVelocity_ = velocity;
    injectionStrength_ = strength;
}

void Fluid2D::pass(int mode, const Target& target, GLuint velocity,
                   GLuint source, GLuint pressure, GLuint divergence,
                   const Fluid2DSettings& settings, float dt,
                   glm::vec2 splatUv, glm::vec4 splatValue) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, Size, Size);
    program_.use();
    glUniform1i(program_.uniform("uMode"), mode);
    glUniform1i(program_.uniform("uViewMode"), settings.viewMode);
    glUniform1f(program_.uniform("uDt"), dt);
    glUniform1f(program_.uniform("uDissipation"), settings.dissipation);
    glUniform1f(program_.uniform("uRadius"), settings.brushRadius);
    glUniform2f(program_.uniform("uTexel"), 1.0f / Size, 1.0f / Size);
    glUniform2f(program_.uniform("uSplatUv"), splatUv.x, splatUv.y);
    glUniform4f(program_.uniform("uSplatValue"),
                splatValue.x, splatValue.y, splatValue.z, splatValue.w);
    const GLuint textures[] = {velocity, source, pressure, divergence, obstacleTexture_};
    for (int i = 0; i < 5; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i] == target.texture ? 0 : textures[i]);
    }
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ++passCount_;
}

void Fluid2D::splat(bool velocity, glm::vec2 uv, glm::vec4 value,
                    const Fluid2DSettings& settings) {
    int& read = velocity ? velocityRead_ : densityRead_;
    Target* targets = velocity ? velocity_ : density_;
    pass(1, targets[1-read], 0, targets[read].texture, 0, 0,
         settings, FixedStep, uv, value);
    read = 1-read;
}

void Fluid2D::step(const Fluid2DSettings& settings, float dt) {
    // 1. Semi-Lagrangian advection of velocity and visible density.
    pass(0, velocity_[1-velocityRead_], velocity_[velocityRead_].texture,
         velocity_[velocityRead_].texture, 0, 0, settings, dt);
    velocityRead_ = 1-velocityRead_;
    pass(0, density_[1-densityRead_], velocity_[velocityRead_].texture,
         density_[densityRead_].texture, 0, 0, settings, dt);
    densityRead_ = 1-densityRead_;

    // 2. External sources: a continuous plume and user-painted impulses.
    if (settings.autoSource) {
        sourceTime_ += dt;
        const glm::vec2 emitter(0.27f + 0.06f * std::sin(sourceTime_ * 1.2f), 0.13f);
        const float strength = settings.sourceStrength * dt;
        splat(true, emitter, glm::vec4(0.04f, settings.force * strength,
              0.0f, 0.0f), settings);
        splat(false, emitter, glm::vec4(0.55f, 0.80f, 1.0f, 1.0f)
              * strength * 9.0f, settings);
    }
    if (pendingInjection_) {
        splat(true, injectionUv_, glm::vec4(injectionVelocity_ * settings.force,
              0.0f, 0.0f), settings);
        splat(false, injectionUv_, glm::vec4(0.8f, 0.4f, 1.0f, 1.0f)
              * injectionStrength_, settings);
        pendingInjection_ = false;
    }

    // 3. Projection: divergence -> Jacobi pressure -> subtract gradient.
    pass(2, divergence_, velocity_[velocityRead_].texture, 0, 0, 0, settings, dt);
    clearTarget(pressure_[0]);
    clearTarget(pressure_[1]);
    pressureRead_ = 0;
    const int iterations = std::clamp(settings.pressureIterations, 4, 80);
    for (int i = 0; i < iterations; ++i) {
        pass(3, pressure_[1-pressureRead_], 0, 0,
             pressure_[pressureRead_].texture, divergence_.texture, settings, dt);
        pressureRead_ = 1-pressureRead_;
    }
    pass(4, velocity_[1-velocityRead_], velocity_[velocityRead_].texture,
         0, pressure_[pressureRead_].texture, 0, settings, dt);
    velocityRead_ = 1-velocityRead_;
}

void Fluid2D::updateDisplay(const Fluid2DSettings& settings) {
    pass(5, display_, velocity_[velocityRead_].texture,
         density_[densityRead_].texture, pressure_[pressureRead_].texture,
         divergence_.texture, settings, FixedStep);
    displayDirty_ = false;
}

void Fluid2D::update(float deltaTime, const Fluid2DSettings& settings) {
    if (!valid_) return;
    const auto started = std::chrono::steady_clock::now();
    passCount_ = 0;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    if (settings.enabled && !settings.paused) {
        accumulator_ += std::clamp(deltaTime, 0.0f, 0.05f);
        int steps = 0;
        while (accumulator_ >= FixedStep && steps < 2) {
            step(settings, FixedStep);
            accumulator_ -= FixedStep;
            ++steps;
        }
        displayDirty_ |= steps > 0;
    }
    // The view mode can change while paused, so the preview is cheap to refresh.
    if (settings.showWindow || displayDirty_) updateDisplay(settings);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    updateMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - started).count();
}

void Fluid2D::destroy() {
    valid_ = false;
    auto release = [](Target& t) {
        glDeleteFramebuffers(1, &t.framebuffer);
        glDeleteTextures(1, &t.texture);
        t = {};
    };
    for (Target& t : velocity_) release(t);
    for (Target& t : density_) release(t);
    for (Target& t : pressure_) release(t);
    release(divergence_);
    release(display_);
    glDeleteTextures(1, &obstacleTexture_);
    obstacleTexture_ = 0;
    glDeleteBuffers(1, &quadVbo_);
    glDeleteVertexArrays(1, &quadVao_);
    quadVbo_ = quadVao_ = 0;
    program_.destroy();
    sceneProgram_.destroy();
}

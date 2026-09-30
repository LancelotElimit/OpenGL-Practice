#pragma once

#include "ShaderProgram.h"
#include "SimulationSettings.h"

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

// Stable-Fluids-style 2D smoke solver using OpenGL 3.3 framebuffer passes.
class Fluid2D {
public:
    explicit Fluid2D(const std::filesystem::path& shaderDirectory);
    ~Fluid2D();
    Fluid2D(const Fluid2D&) = delete;
    Fluid2D& operator=(const Fluid2D&) = delete;

    bool valid() const;
    void reset();
    void resetObstacles(bool centerDisk);
    void paintObstacle(glm::vec2 uv, float radius);
    void inject(glm::vec2 uv, glm::vec2 velocity, float strength);
    void update(float deltaTime, const Fluid2DSettings& settings);
    void drawScene(const glm::mat4& viewProjection, const glm::mat4& model, float opacity);
    GLuint displayTexture() const;
    int resolution() const;
    float updateMilliseconds() const;
    int passCount() const;
    void destroy();

private:
    struct Target {
        GLuint framebuffer = 0;
        GLuint texture = 0;
    };
    static constexpr int Size = 192;
    bool createTarget(Target& target, GLenum format);
    void clearTarget(const Target& target);
    void pass(int mode, const Target& target, GLuint velocity,
              GLuint source, GLuint pressure, GLuint divergence,
              const Fluid2DSettings& settings, float dt,
              glm::vec2 splatUv = {-1.0f, -1.0f},
              glm::vec4 splatValue = glm::vec4(0.0f));
    void splat(bool velocity, glm::vec2 uv, glm::vec4 value,
               const Fluid2DSettings& settings);
    void step(const Fluid2DSettings& settings, float dt);
    void updateDisplay(const Fluid2DSettings& settings);

    ShaderProgram program_;
    ShaderProgram sceneProgram_;
    Target velocity_[2]{};
    Target density_[2]{};
    Target pressure_[2]{};
    Target divergence_{};
    Target display_{};
    GLuint obstacleTexture_ = 0;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    std::vector<std::uint8_t> obstaclePixels_;
    int velocityRead_ = 0;
    int densityRead_ = 0;
    int pressureRead_ = 0;
    float accumulator_ = 0.0f;
    float updateMilliseconds_ = 0.0f;
    int passCount_ = 0;
    float sourceTime_ = 0.0f;
    bool valid_ = false;
    bool displayDirty_ = true;
    bool pendingInjection_ = false;
    glm::vec2 injectionUv_{-1.0f};
    glm::vec2 injectionVelocity_{0.0f};
    float injectionStrength_ = 0.0f;
};

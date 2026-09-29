#pragma once

#include "ShaderProgram.h"

#include <cstddef>
#include <filesystem>

#include <glad/gl.h>
#include <glm/glm.hpp>

struct GpuParticleSettings {
    bool visible = true;
    bool emitting = true;
    bool paused = false;
    int capacity = 8192;
    float emissionRate = 5500.0f;
    float lifetime = 2.5f;
    float gravity = -1.5f;
    float size = 0.055f;
    int preset = 0; // sparks, snow, fountain
};

// OpenGL 3.3 ping-pong Transform Feedback: no CPU per-particle update/readback.
class GpuParticleSystem {
public:
    explicit GpuParticleSystem(const std::filesystem::path& shaderDirectory);
    ~GpuParticleSystem();
    GpuParticleSystem(const GpuParticleSystem&) = delete;
    GpuParticleSystem& operator=(const GpuParticleSystem&) = delete;

    bool valid() const;
    void reset(const GpuParticleSettings& settings);
    void update(float deltaTime, const GpuParticleSettings& settings);
    void draw(const glm::mat4& viewProjection, const glm::mat4& view,
              const GpuParticleSettings& settings) const;
    std::size_t slotCount(const GpuParticleSettings& settings) const;
    void destroy();
    static constexpr int MaxParticles = 32768;

private:
    struct Particle {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float age = 0.0f;
        float lifetime = 1.0f;
        float seed = 0.0f;
    };

    ShaderProgram updateProgram_;
    ShaderProgram renderProgram_;
    GLuint buffers_[2]{};
    GLuint updateVaos_[2]{};
    GLuint renderVaos_[2]{};
    GLuint quadBuffer_ = 0;
    int current_ = 0;
    float time_ = 0.0f;
    bool valid_ = false;
};

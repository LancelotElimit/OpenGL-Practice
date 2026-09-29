#pragma once

#include "ShaderProgram.h"

#include <cstddef>
#include <filesystem>
#include <random>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

struct ParticleSettings {
    bool enabled = true;
    float rate = 45.0f;
    float lifetime = 2.4f;
    float speed = 2.2f;
    float gravity = -2.5f;
    float startSize = 0.12f;
    float endSize = 0.02f;
    int preset = 0; // 0 sparks, 1 smoke, 2 snow
    bool soft = true;
};

class ParticleSystem {
public:
    explicit ParticleSystem(const std::filesystem::path& shaderDirectory);
    ~ParticleSystem();
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    bool valid() const;
    void update(float deltaTime, const ParticleSettings& settings);
    void draw(const glm::mat4& viewProjection, const glm::mat4& view,
              const glm::vec3& cameraPosition, int preset, bool soft);
    std::size_t count() const;
    void clear();
    void destroy();

private:
    struct Particle {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float age = 0.0f;
        float lifetime = 1.0f;
        float startSize = 0.1f;
        float endSize = 0.0f;
    };
    struct Instance {
        glm::vec4 positionSize{0.0f};
        glm::vec4 color{1.0f};
    };

    ShaderProgram program_;
    GLuint vao_ = 0;
    GLuint quadBuffer_ = 0;
    GLuint instanceBuffer_ = 0;
    std::vector<Particle> particles_;
    std::vector<Instance> instances_;
    std::mt19937 random_{0xC0FFEEu};
    float emissionRemainder_ = 0.0f;
    bool valid_ = false;
    static constexpr std::size_t MaxParticles = 4096;
};

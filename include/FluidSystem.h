#pragma once

#include "ShaderProgram.h"
#include "SimulationSettings.h"

#include <cstddef>
#include <filesystem>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

// Small fixed-step WCSPH demonstration with CPU isosurface extraction.
class FluidSystem {
public:
    explicit FluidSystem(const std::filesystem::path& shaderDirectory);
    ~FluidSystem();
    FluidSystem(const FluidSystem&) = delete;
    FluidSystem& operator=(const FluidSystem&) = delete;

    bool valid() const;
    void reset();
    void splash();
    void stepOnce(const FluidSettings& settings);
    void update(float deltaTime, const FluidSettings& settings);
    void draw(const glm::mat4& viewProjection, const glm::vec3& camera,
              const glm::vec3& light, int viewportWidth, int viewportHeight,
              const FluidSettings& settings, const glm::mat4& model,
              const glm::vec3& lightColor, const glm::mat3& environmentRotation,
              float environmentIntensity) const;
    std::size_t particleCount() const;
    std::size_t triangleCount() const;
    float updateMilliseconds() const;
    float simulationMilliseconds() const;
    float surfaceMilliseconds() const;
    void destroy();

private:
    struct Particle {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float density = 1.0f;
        float pressure = 0.0f;
    };
    struct SurfaceVertex {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
    };
    struct FieldSample {
        float value = 0.0f;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 position{0.0f};
    };

    void step(float dt, const FluidSettings& settings);
    void rebuildSurface();
    ShaderProgram program_;
    GLuint surfaceVao_ = 0;
    GLuint surfaceVbo_ = 0;
    GLuint pointsVao_ = 0;
    GLuint pointsVbo_ = 0;
    std::vector<Particle> particles_;
    std::vector<SurfaceVertex> surface_;
    std::vector<FieldSample> field_;
    float restDensity_ = 1.0f;
    float accumulator_ = 0.0f;
    float pourRemainder_ = 0.0f;
    float surfaceTimer_ = 0.0f;
    float updateMilliseconds_ = 0.0f;
    float simulationMilliseconds_ = 0.0f;
    float surfaceMilliseconds_ = 0.0f;
    bool valid_ = false;
    bool surfaceDirty_ = false;
    static constexpr std::size_t MaxParticles = 256;
};

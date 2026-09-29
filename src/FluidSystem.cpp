#include "FluidSystem.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr float smoothingRadius = 0.34f;
constexpr float h2 = smoothingRadius * smoothingRadius;
constexpr float h3 = h2 * smoothingRadius;
constexpr float h6 = h3 * h3;
constexpr float densityScale = 315.0f /
    (64.0f * std::numbers::pi_v<float> * h6 * h3);
constexpr float timestep = 1.0f / 120.0f;
constexpr glm::vec3 boxMin(0.05f, -0.42f, -0.90f);
constexpr glm::vec3 boxMax(2.20f, 1.85f, 0.90f);
constexpr glm::vec3 gridMin(-0.10f, -0.58f, -1.05f);
constexpr float gridStep = 0.115f;
constexpr int gridX = 22, gridY = 23, gridZ = 20;
constexpr float isoValue = 1.08f;
constexpr std::size_t MaxSurfaceTriangles = 80000;

float densityKernel(float r2) {
    if (r2 >= h2) return 0.0f;
    const float d = h2 - r2;
    return densityScale * d * d * d;
}

int fieldIndex(int x, int y, int z) {
    return (z * gridY + y) * gridX + x;
}
} // namespace

FluidSystem::FluidSystem(const std::filesystem::path& shaderDirectory)
    : program_(shaderDirectory / "fluid.vert", shaderDirectory / "fluid.frag",
               "Fluid shader") {
    if (!program_.valid()) return;
    glGenVertexArrays(1, &surfaceVao_);
    glGenBuffers(1, &surfaceVbo_);
    glBindVertexArray(surfaceVao_);
    glBindBuffer(GL_ARRAY_BUFFER, surfaceVbo_);
    glBufferData(GL_ARRAY_BUFFER, MaxSurfaceTriangles * 3 * sizeof(SurfaceVertex),
                 nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SurfaceVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SurfaceVertex),
                          reinterpret_cast<void*>(offsetof(SurfaceVertex, normal)));
    glGenVertexArrays(1, &pointsVao_);
    glGenBuffers(1, &pointsVbo_);
    glBindVertexArray(pointsVao_);
    glBindBuffer(GL_ARRAY_BUFFER, pointsVbo_);
    glBufferData(GL_ARRAY_BUFFER, MaxParticles * sizeof(Particle), nullptr,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), nullptr);
    glBindVertexArray(0);
    valid_ = true;
    reset();
}

FluidSystem::~FluidSystem() { destroy(); }
bool FluidSystem::valid() const { return valid_; }
std::size_t FluidSystem::particleCount() const { return particles_.size(); }
std::size_t FluidSystem::triangleCount() const { return surface_.size() / 3; }
float FluidSystem::updateMilliseconds() const { return updateMilliseconds_; }
float FluidSystem::simulationMilliseconds() const { return simulationMilliseconds_; }
float FluidSystem::surfaceMilliseconds() const { return surfaceMilliseconds_; }

void FluidSystem::reset() {
    particles_.clear();
    // A compact block drops into a bounded basin. The center particle's
    // initial SPH density defines the reference density for this spacing.
    for (int y = 0; y < 5; ++y)
        for (int z = 0; z < 5; ++z)
            for (int x = 0; x < 5; ++x) {
                Particle p;
                p.position = glm::vec3(0.55f + x * 0.165f,
                    0.35f + y * 0.165f, -0.38f + z * 0.165f);
                particles_.push_back(p);
            }
    const glm::vec3 center = particles_[62].position;
    restDensity_ = 0.0f;
    for (const Particle& p : particles_)
        restDensity_ += densityKernel(glm::dot(p.position - center, p.position - center));
    restDensity_ = std::max(restDensity_, 1.0f);
    accumulator_ = pourRemainder_ = surfaceTimer_ = 0.0f;
    rebuildSurface();
    surfaceDirty_ = false;
}

void FluidSystem::splash() {
    const glm::vec3 impact(1.10f, -0.3f, 0.0f);
    for (Particle& p : particles_) {
        glm::vec3 delta = p.position - impact;
        const float r2 = glm::dot(delta, delta);
        if (r2 > 1.2f) continue;
        p.velocity += glm::vec3(delta.x * 3.0f, 3.3f, delta.z * 3.0f)
            * (1.0f - r2 / 1.2f);
    }
}

void FluidSystem::stepOnce(const FluidSettings& settings) {
    if (!valid_) return;
    step(timestep, settings);
    rebuildSurface();
    surfaceDirty_ = false;
}

void FluidSystem::step(float dt, const FluidSettings& settings) {
    // O(N^2) is intentionally explicit for the <=256-particle teaching case.
    for (Particle& p : particles_) {
        p.density = 0.0f;
        for (const Particle& other : particles_)
            p.density += densityKernel(glm::dot(p.position - other.position,
                                                 p.position - other.position));
        p.density = std::max(p.density, 1.0f);
        p.pressure = std::max(p.density - restDensity_, 0.0f)
            * std::clamp(settings.pressure, 0.0f, 50.0f);
    }

    std::vector<glm::vec3> accelerations(particles_.size(),
        glm::vec3(0.0f, -std::clamp(settings.gravity, 0.0f, 25.0f), 0.0f));
    const float spiky = -45.0f / (std::numbers::pi_v<float> * h6);
    const float laplacian = -spiky;
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        for (std::size_t j = i + 1; j < particles_.size(); ++j) {
            const glm::vec3 difference = particles_[i].position - particles_[j].position;
            const float r2 = glm::dot(difference, difference);
            if (r2 >= h2 || r2 < 1e-10f) continue;
            const float r = std::sqrt(r2);
            const glm::vec3 direction = difference / r;
            const float pressureTerm = (particles_[i].pressure + particles_[j].pressure)
                / (2.0f * particles_[i].density * particles_[j].density);
            const glm::vec3 pressureAcceleration = -pressureTerm * spiky
                * (smoothingRadius - r) * (smoothingRadius - r) * direction;
            // Pairwise symmetric pressure and a simple viscosity Laplacian.
            accelerations[i] += pressureAcceleration;
            accelerations[j] -= pressureAcceleration;
            const glm::vec3 viscous = std::clamp(settings.viscosity, 0.0f, 1.0f)
                * laplacian * (smoothingRadius - r)
                * (particles_[j].velocity - particles_[i].velocity)
                / (particles_[i].density * particles_[j].density);
            accelerations[i] += viscous;
            accelerations[j] -= viscous;
        }
    }
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        glm::vec3 acceleration = accelerations[i];
        const float magnitude = glm::length(acceleration);
        if (magnitude > 80.0f) acceleration *= 80.0f / magnitude;
        Particle& p = particles_[i];
        p.velocity += acceleration * dt;
        p.velocity *= 0.998f;
        const float speed = glm::length(p.velocity);
        if (speed > 7.0f) p.velocity *= 7.0f / speed;
        p.position += p.velocity * dt;
        for (int axis = 0; axis < 3; ++axis) {
            if (p.position[axis] < boxMin[axis]) {
                p.position[axis] = boxMin[axis];
                if (p.velocity[axis] < 0.0f) p.velocity[axis] *= -0.18f;
            } else if (p.position[axis] > boxMax[axis]) {
                p.position[axis] = boxMax[axis];
                if (p.velocity[axis] > 0.0f) p.velocity[axis] *= -0.18f;
            }
        }
        if (p.position.y <= boxMin.y + 0.001f) {
            p.velocity.x *= 0.985f;
            p.velocity.z *= 0.985f;
        }
    }
}

void FluidSystem::update(float deltaTime, const FluidSettings& settings) {
    if (!valid_ || !settings.visible) return;
    if (settings.paused) {
        if (settings.visible && settings.viewMode != 1 && surfaceDirty_) {
            rebuildSurface();
            surfaceDirty_ = false;
        }
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const float frameStep = std::clamp(deltaTime, 0.0f, 0.05f);
    accumulator_ += frameStep;
    surfaceTimer_ += frameStep;
    int steps = 0;
    while (accumulator_ >= timestep && steps < 6) {
        step(timestep, settings);
        accumulator_ -= timestep;
        ++steps;
    }
    simulationMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    if (settings.pour && particles_.size() < MaxParticles) {
        pourRemainder_ += std::clamp(settings.pourRate, 0.0f, 100.0f)
            * std::clamp(deltaTime, 0.0f, 0.05f);
        while (pourRemainder_ >= 1.0f && particles_.size() < MaxParticles) {
            const float phase = static_cast<float>(particles_.size()) * 2.399963f;
            Particle p;
            p.position = glm::vec3(0.40f + 0.08f * std::cos(phase),
                                   1.78f, 0.08f * std::sin(phase));
            p.velocity = glm::vec3(0.65f, -0.4f, 0.0f);
            particles_.push_back(p);
            pourRemainder_ -= 1.0f;
        }
    }
    if (steps > 0 || settings.pour) surfaceDirty_ = true;
    if (settings.visible && settings.viewMode != 1 && surfaceDirty_
        && surfaceTimer_ >= 1.0f / std::clamp(settings.surfaceHz, 5, 60)) {
        rebuildSurface();
        surfaceDirty_ = false;
        surfaceTimer_ = 0.0f;
    } else if (settings.visible && settings.viewMode == 1) {
        glBindBuffer(GL_ARRAY_BUFFER, pointsVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, particles_.size() * sizeof(Particle),
                        particles_.data());
    }
    updateMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

void FluidSystem::rebuildSurface() {
    const auto surfaceStart = std::chrono::steady_clock::now();
    field_.resize(static_cast<std::size_t>(gridX) * gridY * gridZ);
    for (int z = 0; z < gridZ; ++z)
        for (int y = 0; y < gridY; ++y)
            for (int x = 0; x < gridX; ++x) {
                FieldSample& sample = field_[fieldIndex(x, y, z)];
                sample.position = gridMin + gridStep * glm::vec3(x, y, z);
                sample.value = 0.0f;
                sample.normal = glm::vec3(0.0f);
            }
    // Splat each compact-support kernel only into nearby voxels. Scanning
    // every particle for every voxel was too slow in Visual Studio Debug.
    for (const Particle& p : particles_) {
        const glm::vec3 first = (p.position - glm::vec3(smoothingRadius)
            - gridMin) / gridStep;
        const glm::vec3 last = (p.position + glm::vec3(smoothingRadius)
            - gridMin) / gridStep;
        const int x0 = std::max(0, static_cast<int>(std::floor(first.x)));
        const int y0 = std::max(0, static_cast<int>(std::floor(first.y)));
        const int z0 = std::max(0, static_cast<int>(std::floor(first.z)));
        const int x1 = std::min(gridX - 1, static_cast<int>(std::ceil(last.x)));
        const int y1 = std::min(gridY - 1, static_cast<int>(std::ceil(last.y)));
        const int z1 = std::min(gridZ - 1, static_cast<int>(std::ceil(last.z)));
        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    FieldSample& sample = field_[fieldIndex(x, y, z)];
                    if (sample.position.y < -0.49f) continue;
                    const glm::vec3 delta = sample.position - p.position;
                    const float r2 = glm::dot(delta, delta);
                    if (r2 >= h2) continue;
                    const float q = 1.0f - r2 / h2;
                    sample.value += q * q * q;
                    sample.normal += 6.0f / h2 * q * q * delta;
                }
    }
    for (FieldSample& sample : field_) {
        if (glm::dot(sample.normal, sample.normal) > 1e-8f)
            sample.normal = glm::normalize(sample.normal);
        else sample.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    surface_.clear();
    constexpr int corners[8][3] = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    };
    constexpr int tetrahedra[6][4] = {
        {0,5,1,6}, {0,1,2,6}, {0,2,3,6},
        {0,3,7,6}, {0,7,4,6}, {0,4,5,6}
    };
    auto crossing = [](const FieldSample& a, const FieldSample& b) {
        const float denominator = b.value - a.value;
        const float t = std::abs(denominator) > 1e-6f
            ? glm::clamp((isoValue - a.value) / denominator, 0.0f, 1.0f)
            : 0.5f;
        SurfaceVertex v;
        v.position = glm::mix(a.position, b.position, t);
        v.normal = glm::normalize(glm::mix(a.normal, b.normal, t)
            + glm::vec3(0.0f, 1e-5f, 0.0f));
        return v;
    };
    auto triangle = [&](SurfaceVertex a, SurfaceVertex b, SurfaceVertex c) {
        if (surface_.size() / 3 >= MaxSurfaceTriangles) return;
        const glm::vec3 n = a.normal + b.normal + c.normal;
        if (glm::dot(glm::cross(b.position - a.position, c.position - a.position), n) < 0.0f)
            std::swap(b, c);
        surface_.push_back(a);
        surface_.push_back(b);
        surface_.push_back(c);
    };
    for (int z = 0; z < gridZ - 1; ++z)
        for (int y = 0; y < gridY - 1; ++y)
            for (int x = 0; x < gridX - 1; ++x) {
                const FieldSample* cube[8];
                float lo = 1e20f, hi = -1e20f;
                for (int c = 0; c < 8; ++c) {
                    cube[c] = &field_[fieldIndex(x + corners[c][0],
                        y + corners[c][1], z + corners[c][2])];
                    lo = std::min(lo, cube[c]->value);
                    hi = std::max(hi, cube[c]->value);
                }
                if (lo >= isoValue || hi < isoValue) continue;
                for (const auto& tet : tetrahedra) {
                    const FieldSample* inside[4];
                    const FieldSample* outside[4];
                    int inCount = 0, outCount = 0;
                    for (int c : tet) {
                        if (cube[c]->value >= isoValue) inside[inCount++] = cube[c];
                        else outside[outCount++] = cube[c];
                    }
                    if (inCount == 1) {
                        triangle(crossing(*inside[0], *outside[0]),
                                 crossing(*inside[0], *outside[1]),
                                 crossing(*inside[0], *outside[2]));
                    } else if (inCount == 3) {
                        triangle(crossing(*outside[0], *inside[0]),
                                 crossing(*outside[0], *inside[1]),
                                 crossing(*outside[0], *inside[2]));
                    } else if (inCount == 2) {
                        const auto ac = crossing(*inside[0], *outside[0]);
                        const auto ad = crossing(*inside[0], *outside[1]);
                        const auto bc = crossing(*inside[1], *outside[0]);
                        const auto bd = crossing(*inside[1], *outside[1]);
                        triangle(ac, ad, bc);
                        triangle(ad, bd, bc);
                    }
                }
            }
    glBindBuffer(GL_ARRAY_BUFFER, surfaceVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, surface_.size() * sizeof(SurfaceVertex),
                    surface_.data());
    glBindBuffer(GL_ARRAY_BUFFER, pointsVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particles_.size() * sizeof(Particle),
                    particles_.data());
    surfaceMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - surfaceStart).count();
}

void FluidSystem::draw(const glm::mat4& viewProjection, const glm::vec3& camera,
                       const glm::vec3& light, int viewportWidth,
                       int viewportHeight, const FluidSettings& settings) const {
    if (!valid_ || !settings.visible) return;
    program_.use();
    glUniformMatrix4fv(program_.uniform("uViewProjection"), 1, GL_FALSE,
                       glm::value_ptr(viewProjection));
    glUniform3fv(program_.uniform("uCamera"), 1, glm::value_ptr(camera));
    glUniform3fv(program_.uniform("uLight"), 1, glm::value_ptr(light));
    glUniform1f(program_.uniform("uOpacity"),
                glm::clamp(settings.opacity, 0.05f, 1.0f));
    glUniform2f(program_.uniform("uViewport"),
                static_cast<float>(viewportWidth),
                static_cast<float>(viewportHeight));
    glUniform1f(program_.uniform("uRefraction"), settings.refraction);
    glUniform1f(program_.uniform("uAbsorption"), settings.absorption);
    glUniform1f(program_.uniform("uReflectivity"), settings.reflectivity);
    glUniform1f(program_.uniform("uFoam"), settings.foam);
    glUniform1f(program_.uniform("uRoughness"), settings.roughness);
    glUniform1i(program_.uniform("uShadingMode"), settings.shadingMode);
    glUniform1i(program_.uniform("uSceneColor"), 0);
    glUniform1i(program_.uniform("uSceneDepth"), 1);
    glUniform1i(program_.uniform("uEnvironment"), 4);
    if (settings.viewMode == 1) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniform1i(program_.uniform("uPoints"), GL_TRUE);
        glBindVertexArray(pointsVao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particles_.size()));
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glUniform1i(program_.uniform("uPoints"), GL_FALSE);
        glBindVertexArray(surfaceVao_);
        glEnable(GL_CULL_FACE);
        if (settings.viewMode == 2) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(surface_.size()));
        if (settings.viewMode == 2) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_CULL_FACE);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void FluidSystem::destroy() {
    valid_ = false;
    glDeleteBuffers(1, &surfaceVbo_);
    glDeleteBuffers(1, &pointsVbo_);
    glDeleteVertexArrays(1, &surfaceVao_);
    glDeleteVertexArrays(1, &pointsVao_);
    surfaceVbo_ = pointsVbo_ = surfaceVao_ = pointsVao_ = 0;
    program_.destroy();
}

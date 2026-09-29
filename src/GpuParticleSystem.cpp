#include "GpuParticleSystem.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

GpuParticleSystem::GpuParticleSystem(const std::filesystem::path& shaderDirectory)
    : updateProgram_(shaderDirectory / "gpu_particle_update.vert",
                     shaderDirectory / "gpu_particle_update.frag",
                     "GPU particle update",
                     {"outPosition", "outVelocity", "outAge", "outLifetime", "outSeed"}),
      renderProgram_(shaderDirectory / "gpu_particle.vert",
                     shaderDirectory / "gpu_particle.frag", "GPU particle render") {
    static_assert(sizeof(Particle) == 9 * sizeof(float));
    if (!updateProgram_.valid() || !renderProgram_.valid()) return;

    constexpr float corners[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, 1.0f
    };
    glGenBuffers(1, &quadBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glGenBuffers(2, buffers_);
    glGenVertexArrays(2, updateVaos_);
    glGenVertexArrays(2, renderVaos_);
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers_[i]);
        glBufferData(GL_ARRAY_BUFFER, MaxParticles * sizeof(Particle), nullptr,
                     GL_DYNAMIC_COPY);

        glBindVertexArray(updateVaos_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, buffers_[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Particle),
                              reinterpret_cast<void*>(offsetof(Particle, velocity)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Particle),
                              reinterpret_cast<void*>(offsetof(Particle, age)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Particle),
                              reinterpret_cast<void*>(offsetof(Particle, lifetime)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Particle),
                              reinterpret_cast<void*>(offsetof(Particle, seed)));

        glBindVertexArray(renderVaos_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, quadBuffer_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, buffers_[i]);
        const int components[] = {3, 3, 1, 1, 1};
        const std::size_t offsets[] = {
            offsetof(Particle, position), offsetof(Particle, velocity),
            offsetof(Particle, age), offsetof(Particle, lifetime),
            offsetof(Particle, seed)
        };
        for (int attribute = 0; attribute < 5; ++attribute) {
            glEnableVertexAttribArray(1 + attribute);
            glVertexAttribPointer(1 + attribute, components[attribute], GL_FLOAT,
                                  GL_FALSE, sizeof(Particle),
                                  reinterpret_cast<void*>(offsets[attribute]));
            glVertexAttribDivisor(1 + attribute, 1);
        }
    }
    glBindVertexArray(0);
    valid_ = true;
    reset(GpuParticleSettings{});
}

GpuParticleSystem::~GpuParticleSystem() { destroy(); }
bool GpuParticleSystem::valid() const { return valid_; }
std::size_t GpuParticleSystem::slotCount(const GpuParticleSettings& settings) const {
    return static_cast<std::size_t>(std::clamp(settings.capacity, 1, MaxParticles));
}

void GpuParticleSystem::reset(const GpuParticleSettings& settings) {
    if (!valid_) return;
    std::vector<Particle> particles(MaxParticles);
    const float rate = std::max(settings.emissionRate, 1.0f);
    for (int i = 0; i < MaxParticles; ++i) {
        particles[i].position = glm::vec3(-1.45f, -0.2f, 0.25f);
        particles[i].age = -static_cast<float>(i) / rate;
        particles[i].lifetime = std::max(settings.lifetime, 0.1f);
        particles[i].seed = static_cast<float>(i) * 0.6180339f;
    }
    for (GLuint buffer : buffers_) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, MaxParticles * sizeof(Particle),
                        particles.data());
    }
    current_ = 0;
    time_ = 0.0f;
}

void GpuParticleSystem::update(float deltaTime, const GpuParticleSettings& settings) {
    if (!valid_ || settings.paused || !settings.visible) return;
    const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
    if (dt <= 0.0f) return;
    time_ += dt;
    const int next = 1 - current_;
    updateProgram_.use();
    glUniform1f(updateProgram_.uniform("uDt"), dt);
    glUniform1f(updateProgram_.uniform("uTime"), time_);
    glUniform1f(updateProgram_.uniform("uLifetime"),
                std::max(0.1f, settings.lifetime));
    glUniform1f(updateProgram_.uniform("uGravity"), settings.gravity);
    glUniform1f(updateProgram_.uniform("uRate"),
                std::max(1.0f, settings.emissionRate));
    glUniform1i(updateProgram_.uniform("uCount"),
                static_cast<GLint>(slotCount(settings)));
    glUniform1i(updateProgram_.uniform("uPreset"), settings.preset);
    glUniform1i(updateProgram_.uniform("uEmitting"), settings.emitting ? 1 : 0);
    glEnable(GL_RASTERIZER_DISCARD);
    glBindVertexArray(updateVaos_[current_]);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers_[next]);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(slotCount(settings)));
    glEndTransformFeedback();
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glBindVertexArray(0);
    glDisable(GL_RASTERIZER_DISCARD);
    current_ = next;
}

void GpuParticleSystem::draw(const glm::mat4& viewProjection,
                             const glm::mat4& view,
                             const GpuParticleSettings& settings) const {
    if (!valid_ || !settings.visible) return;
    renderProgram_.use();
    glUniformMatrix4fv(renderProgram_.uniform("uViewProjection"), 1, GL_FALSE,
                       glm::value_ptr(viewProjection));
    glUniform3fv(renderProgram_.uniform("uCameraRight"), 1,
                 glm::value_ptr(glm::vec3(view[0][0], view[1][0], view[2][0])));
    glUniform3fv(renderProgram_.uniform("uCameraUp"), 1,
                 glm::value_ptr(glm::vec3(view[0][1], view[1][1], view[2][1])));
    glUniform1f(renderProgram_.uniform("uSize"), settings.size);
    glUniform1i(renderProgram_.uniform("uPreset"), settings.preset);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(renderVaos_[current_]);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                          static_cast<GLsizei>(slotCount(settings)));
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void GpuParticleSystem::destroy() {
    if (!valid_) return;
    valid_ = false;
    glDeleteVertexArrays(2, updateVaos_);
    glDeleteVertexArrays(2, renderVaos_);
    glDeleteBuffers(2, buffers_);
    glDeleteBuffers(1, &quadBuffer_);
    updateVaos_[0] = updateVaos_[1] = 0;
    renderVaos_[0] = renderVaos_[1] = 0;
    buffers_[0] = buffers_[1] = quadBuffer_ = 0;
    updateProgram_.destroy();
    renderProgram_.destroy();
}

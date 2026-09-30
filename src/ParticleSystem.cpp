#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <glm/gtc/type_ptr.hpp>

ParticleSystem::ParticleSystem(const std::filesystem::path& shaderDirectory)
    : program_(shaderDirectory / "particle.vert",
               shaderDirectory / "particle.frag", "Particle shader") {
    if (!program_.valid()) return;
    constexpr float quad[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, 1.0f
    };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &quadBuffer_);
    glGenBuffers(1, &instanceBuffer_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    glBufferData(GL_ARRAY_BUFFER, MaxParticles * sizeof(Instance), nullptr,
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), nullptr);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                          reinterpret_cast<void*>(offsetof(Instance, color)));
    glVertexAttribDivisor(2, 1);
    glBindVertexArray(0);
    valid_ = true;
}

ParticleSystem::~ParticleSystem() { destroy(); }
bool ParticleSystem::valid() const { return valid_; }
std::size_t ParticleSystem::count() const { return particles_.size(); }

void ParticleSystem::update(float deltaTime, const ParticleSettings& settings) {
    // Clamp a resumed/minimized window's delta so it cannot release a burst.
    const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
    for (Particle& p : particles_) {
        p.age += dt;
        p.velocity.y += settings.gravity * dt;
        p.position += p.velocity * dt;
    }
    std::erase_if(particles_, [](const Particle& p) { return p.age >= p.lifetime; });
    if (!settings.enabled) return;

    emissionRemainder_ += std::clamp(settings.rate, 0.0f, 1000.0f) * dt;
    const int requested = static_cast<int>(emissionRemainder_);
    emissionRemainder_ -= static_cast<float>(requested);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    for (int i = 0; i < requested && particles_.size() < MaxParticles; ++i) {
        Particle p;
        p.position = glm::vec3(0.0f); // local to the owning emitter
        p.lifetime = std::max(0.1f, settings.lifetime)
            * (0.75f + 0.25f * (unit(random_) + 1.0f));
        p.startSize = std::max(0.001f, settings.startSize);
        p.endSize = std::max(0.001f, settings.endSize);
        if (settings.preset == 1) { // rising smoke
            p.velocity = glm::vec3(unit(random_) * 0.35f,
                                   settings.speed * 0.55f,
                                   unit(random_) * 0.35f);
        } else if (settings.preset == 2) { // drifting snow
            p.position += glm::vec3(unit(random_) * 2.0f, 2.5f,
                                    unit(random_) * 2.0f);
            p.velocity = glm::vec3(unit(random_) * 0.2f,
                                   -settings.speed * 0.45f,
                                   unit(random_) * 0.2f);
        } else { // sparks
            p.velocity = glm::vec3(unit(random_) * 0.8f,
                                   0.5f + 0.5f * (unit(random_) + 1.0f),
                                   unit(random_) * 0.8f) * settings.speed;
        }
        particles_.push_back(p);
    }
}

void ParticleSystem::draw(const glm::mat4& viewProjection,
                          const glm::mat4& view,
                          const glm::vec3& cameraPosition, int preset, bool soft,
                          const glm::mat4& emitterTransform) {
    if (!valid_ || particles_.empty()) return;
    // Back-to-front order is needed for standard alpha compositing.
    std::vector<const Particle*> sorted;
    sorted.reserve(particles_.size());
    for (const Particle& p : particles_) sorted.push_back(&p);
    std::sort(sorted.begin(), sorted.end(), [&](const Particle* a, const Particle* b) {
        const glm::vec3 da = glm::vec3(emitterTransform * glm::vec4(a->position,1)) - cameraPosition;
        const glm::vec3 db = glm::vec3(emitterTransform * glm::vec4(b->position,1)) - cameraPosition;
        return glm::dot(da, da) > glm::dot(db, db);
    });
    instances_.clear();
    for (const Particle* p : sorted) {
        const float t = std::clamp(p->age / p->lifetime, 0.0f, 1.0f);
        const float size = glm::mix(p->startSize, p->endSize, t);
        glm::vec4 color;
        if (preset == 1) color = glm::vec4(0.65f, 0.68f, 0.72f, 0.35f * (1.0f - t));
        else if (preset == 2) color = glm::vec4(0.78f, 0.88f, 1.0f, 0.7f * (1.0f - t));
        else color = glm::vec4(3.0f, 1.2f * (1.0f - t) + 0.1f, 0.08f,
                               0.85f * (1.0f - t));
        const auto worldPosition = glm::vec3(emitterTransform * glm::vec4(p->position,1));
        const float sizeScale = std::max({glm::length(glm::vec3(emitterTransform[0])),
            glm::length(glm::vec3(emitterTransform[1])), glm::length(glm::vec3(emitterTransform[2]))});
        instances_.push_back({glm::vec4(worldPosition, size * sizeScale), color});
    }
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instances_.size() * sizeof(Instance),
                    instances_.data());
    program_.use();
    glUniform1i(program_.uniform("uSceneDepth"), 0);
    glUniform1i(program_.uniform("uSoft"), soft ? GL_TRUE : GL_FALSE);
    glUniformMatrix4fv(program_.uniform("uViewProjection"), 1, GL_FALSE,
                       glm::value_ptr(viewProjection));
    glUniform3fv(program_.uniform("uCameraRight"), 1,
                 glm::value_ptr(glm::vec3(view[0][0], view[1][0], view[2][0])));
    glUniform3fv(program_.uniform("uCameraUp"), 1,
                 glm::value_ptr(glm::vec3(view[0][1], view[1][1], view[2][1])));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(instances_.size()));
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleSystem::clear() {
    particles_.clear();
    emissionRemainder_ = 0.0f;
}

void ParticleSystem::destroy() {
    valid_ = false;
    glDeleteBuffers(1, &instanceBuffer_);
    glDeleteBuffers(1, &quadBuffer_);
    glDeleteVertexArrays(1, &vao_);
    instanceBuffer_ = quadBuffer_ = vao_ = 0;
    program_.destroy();
}

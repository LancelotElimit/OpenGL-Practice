#pragma once

#include <array>

#include <glm/glm.hpp>

class Scene {
public:
    void setModelImportTransform(const glm::mat4& transform);
    glm::vec3& modelPosition();
    glm::vec3& modelRotation();
    glm::vec3& modelScale();
    void update(float timeSeconds);

    const std::array<glm::mat4, 1>& modelTransforms() const;
    const glm::mat4& floorTransform() const;
    const glm::vec3& primaryLightPosition() const;
    const glm::vec3& secondaryLightPosition() const;

private:
    glm::mat4 modelImportTransform_{1.0f};
    glm::vec3 modelPosition_{0.0f};
    glm::vec3 modelRotation_{0.0f};
    glm::vec3 modelScale_{1.0f};
    std::array<glm::mat4, 1> modelTransforms_{};
    glm::mat4 floorTransform_{1.0f};
    glm::vec3 primaryLightPosition_{2.0f, 1.5f, 0.0f};
    glm::vec3 secondaryLightPosition_{-2.0f, 0.75f, 0.0f};
};

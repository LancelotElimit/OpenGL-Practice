#pragma once

#include <array>

#include <glm/glm.hpp>

class Scene {
public:
    void update(float timeSeconds);

    const std::array<glm::mat4, 3>& modelTransforms() const;
    const glm::mat4& floorTransform() const;
    const glm::vec3& primaryLightPosition() const;
    const glm::vec3& secondaryLightPosition() const;

private:
    std::array<glm::mat4, 3> modelTransforms_{};
    glm::mat4 floorTransform_{1.0f};
    glm::vec3 primaryLightPosition_{2.0f, 1.5f, 0.0f};
    glm::vec3 secondaryLightPosition_{-2.0f, 0.75f, 0.0f};
};

#pragma once

#include <array>

#include <glm/glm.hpp>

class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);
    bool containsSphere(const glm::vec3& center, float radius) const;

private:
    std::array<glm::vec4, 6> planes_{};
};

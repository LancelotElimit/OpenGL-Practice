#include "Frustum.h"

namespace {

glm::vec4 normalizedPlane(const glm::vec4& plane) {
    const float length = glm::length(glm::vec3(plane));
    return length > 0.0f ? plane / length : plane;
}

} // namespace

Frustum::Frustum(const glm::mat4& matrix) {
    const glm::vec4 row0(
        matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0]
    );
    const glm::vec4 row1(
        matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1]
    );
    const glm::vec4 row2(
        matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2]
    );
    const glm::vec4 row3(
        matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3]
    );
    planes_[0] = normalizedPlane(row3 + row0);
    planes_[1] = normalizedPlane(row3 - row0);
    planes_[2] = normalizedPlane(row3 + row1);
    planes_[3] = normalizedPlane(row3 - row1);
    planes_[4] = normalizedPlane(row3 + row2);
    planes_[5] = normalizedPlane(row3 - row2);
}

bool Frustum::containsSphere(
    const glm::vec3& center,
    float radius
) const {
    for (const glm::vec4& plane : planes_) {
        const float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

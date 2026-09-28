#include "Scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {

struct SceneNode {
    glm::mat4 localTransform{1.0f};
    int parentIndex = -1;
};

} // namespace

void Scene::update(float timeSeconds) {
    primaryLightPosition_ = glm::vec3(
        2.0f * glm::cos(timeSeconds),
        1.5f,
        2.0f * glm::sin(timeSeconds)
    );
    secondaryLightPosition_ = glm::vec3(
        -2.0f * glm::cos(timeSeconds),
        0.75f,
        -2.0f * glm::sin(timeSeconds)
    );

    const std::array<SceneNode, 3> nodes = {{
        {
            glm::rotate(
                glm::mat4(1.0f),
                timeSeconds,
                glm::vec3(0.5f, 1.0f, 0.0f)
            ),
            -1
        },
        {
            glm::translate(
                glm::mat4(1.0f),
                glm::vec3(-1.4f, -0.05f, 0.0f)
            ) * glm::rotate(
                glm::mat4(1.0f),
                -0.8f * timeSeconds,
                glm::vec3(0.0f, 1.0f, 0.5f)
            ) * glm::scale(
                glm::mat4(1.0f),
                glm::vec3(0.65f)
            ),
            0
        },
        {
            glm::translate(
                glm::mat4(1.0f),
                glm::vec3(1.4f, 0.15f, -0.6f)
            ) * glm::rotate(
                glm::mat4(1.0f),
                1.3f * timeSeconds,
                glm::vec3(1.0f, 0.0f, 0.5f)
            ) * glm::scale(
                glm::mat4(1.0f),
                glm::vec3(0.45f)
            ),
            0
        }
    }};

    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const int parentIndex = nodes[index].parentIndex;
        modelTransforms_[index] = parentIndex < 0
            ? nodes[index].localTransform
            : modelTransforms_[static_cast<std::size_t>(parentIndex)]
                * nodes[index].localTransform;
    }
}

const std::array<glm::mat4, 3>& Scene::modelTransforms() const {
    return modelTransforms_;
}

const glm::mat4& Scene::floorTransform() const {
    return floorTransform_;
}

const glm::vec3& Scene::primaryLightPosition() const {
    return primaryLightPosition_;
}

const glm::vec3& Scene::secondaryLightPosition() const {
    return secondaryLightPosition_;
}

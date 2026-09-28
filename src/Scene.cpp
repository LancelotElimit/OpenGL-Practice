#include "Scene.h"

#include <glm/gtc/matrix_transform.hpp>

void Scene::setModelImportTransform(const glm::mat4& transform) {
    modelImportTransform_ = transform;
}

void Scene::update(float) {
    // Keep the inspection scene static: only the camera is interactive.
    primaryLightPosition_ = glm::vec3(2.0f, 1.5f, 2.0f);
    secondaryLightPosition_ = glm::vec3(-2.0f, 0.75f, -2.0f);

    // The inspection scene contains exactly one stationary imported model.
    modelTransforms_[0] = modelImportTransform_;
}

const std::array<glm::mat4, 1>& Scene::modelTransforms() const {
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

#include "Scene.h"

#include <glm/gtc/matrix_transform.hpp>

void Scene::setModelImportTransform(const glm::mat4& transform) {
    modelImportTransform_ = transform;
}

glm::vec3& Scene::modelPosition() { return modelPosition_; }
glm::vec3& Scene::modelRotation() { return modelRotation_; }
glm::vec3& Scene::modelScale() { return modelScale_; }

void Scene::update(float) {
    // Keep the inspection scene static: only the camera is interactive.
    primaryLightPosition_ = glm::vec3(2.0f, 1.5f, 2.0f);
    secondaryLightPosition_ = glm::vec3(-2.0f, 0.75f, -2.0f);

    // The inspection scene contains exactly one stationary imported model.
    const glm::mat4 editorTransform =
        glm::translate(glm::mat4(1.0f), modelPosition_)
        * glm::rotate(glm::mat4(1.0f), glm::radians(modelRotation_.z), glm::vec3(0, 0, 1))
        * glm::rotate(glm::mat4(1.0f), glm::radians(modelRotation_.y), glm::vec3(0, 1, 0))
        * glm::rotate(glm::mat4(1.0f), glm::radians(modelRotation_.x), glm::vec3(1, 0, 0))
        * glm::scale(glm::mat4(1.0f), modelScale_);
    modelTransforms_[0] = editorTransform * modelImportTransform_;
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

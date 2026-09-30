#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : position_(position), yaw_(yaw), pitch_(pitch) {
    updateFrontFromAngles();
}

void Camera::move(float rightAxis, float forwardAxis, float deltaTime) {
    const float velocity = 2.5f * deltaTime;
    const glm::vec3 right =
        glm::normalize(glm::cross(front_, worldUp_));

    position_ += velocity * (front_ * forwardAxis + right * rightAxis);
}

void Camera::rotateFromPointer(double mouseX, double mouseY) {

    if (firstMouseSample_) {
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
        firstMouseSample_ = false;
    }

    const float xOffset = static_cast<float>(mouseX - lastMouseX_);
    const float yOffset = static_cast<float>(lastMouseY_ - mouseY);
    lastMouseX_ = mouseX;
    lastMouseY_ = mouseY;

    constexpr float sensitivity = 0.1f;
    yaw_ += xOffset * sensitivity;
    pitch_ += yOffset * sensitivity;
    pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
    updateFrontFromAngles();
}

void Camera::resetMouseSample() {
    firstMouseSample_ = true;
}

void Camera::lookAt(const glm::vec3& position, const glm::vec3& target) {
    if(glm::length(target-position)<1e-5f) return;
    position_ = position;
    const glm::vec3 direction = glm::normalize(target - position);
    yaw_ = glm::degrees(std::atan2(direction.z, direction.x));
    pitch_ = glm::clamp(glm::degrees(std::asin(glm::clamp(direction.y, -1.0f, 1.0f))),-89.f,89.f);
    updateFrontFromAngles();
    resetMouseSample();
}

void Camera::updateFrontFromAngles() {
    glm::vec3 direction;
    direction.x = glm::cos(glm::radians(yaw_))
        * glm::cos(glm::radians(pitch_));
    direction.y = glm::sin(glm::radians(pitch_));
    direction.z = glm::sin(glm::radians(yaw_))
        * glm::cos(glm::radians(pitch_));
    front_ = glm::normalize(direction);
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + front_, worldUp_);
}

const glm::vec3& Camera::position() const {
    return position_;
}

const glm::vec3& Camera::front() const {
    return front_;
}

#pragma once

struct GLFWwindow;

#include <glm/glm.hpp>

class Camera {
public:
    explicit Camera(
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
        float yaw = -90.0f,
        float pitch = 0.0f
    );

    void processKeyboard(GLFWwindow* window, float deltaTime);
    void processMouse(GLFWwindow* window);
    void resetMouseSample();
    void lookAt(const glm::vec3& position, const glm::vec3& target);

    glm::mat4 viewMatrix() const;
    const glm::vec3& position() const;
    const glm::vec3& front() const;
    void setFieldOfView(float value) { fieldOfView_=glm::clamp(value,20.f,100.f); }
    float fieldOfView() const { return fieldOfView_; }

private:
    void updateFrontFromAngles();

    glm::vec3 position_;
    float fieldOfView_ = 45;
    glm::vec3 front_{0.0f, 0.0f, -1.0f};
    glm::vec3 worldUp_{0.0f, 1.0f, 0.0f};
    float yaw_ = -90.0f;
    float pitch_ = 0.0f;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    bool firstMouseSample_ = true;
};

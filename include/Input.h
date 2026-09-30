#pragma once
#include <glm/vec2.hpp>
struct GLFWwindow;
struct InputSnapshot {
    bool focused=false, escape=false, toggleBloom=false, toggleCamera=false;
    bool shadowPreview=false, grayscale=false, control=false, alt=false;
    glm::vec2 movement{0};
    float exposureAxis=0, metallicAxis=0, roughnessAxis=0;
    double mouseX=0, mouseY=0;
};
// Raw platform sampling, without any editor/game routing policy.
class InputSystem {
public:
    InputSnapshot sample(GLFWwindow* window);
private:
    bool escapeDown_=false,bloomDown_=false,cameraDown_=false;
};

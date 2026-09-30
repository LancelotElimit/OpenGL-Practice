#include "Input.h"
#include <GLFW/glfw3.h>
InputSnapshot InputSystem::sample(GLFWwindow* window) {
    InputSnapshot result;
    const auto down=[window](int key){return glfwGetKey(window,key)==GLFW_PRESS;};
    const auto edge=[](bool value,bool& previous){const bool pressed=value&&!previous;previous=value;return pressed;};
    result.focused=glfwGetWindowAttrib(window,GLFW_FOCUSED)!=0;
    result.escape=edge(down(GLFW_KEY_ESCAPE),escapeDown_);
    result.toggleBloom=edge(down(GLFW_KEY_F3),bloomDown_);
    result.toggleCamera=edge(down(GLFW_KEY_F4),cameraDown_);
    if(!result.focused) return {};
    result.shadowPreview=down(GLFW_KEY_F1); result.grayscale=down(GLFW_KEY_F2);
    result.control=down(GLFW_KEY_LEFT_CONTROL)||down(GLFW_KEY_RIGHT_CONTROL);
    result.alt=down(GLFW_KEY_LEFT_ALT)||down(GLFW_KEY_RIGHT_ALT);
    result.movement={float(down(GLFW_KEY_D))-float(down(GLFW_KEY_A)),float(down(GLFW_KEY_W))-float(down(GLFW_KEY_S))};
    result.exposureAxis=float(down(GLFW_KEY_UP))-float(down(GLFW_KEY_DOWN));
    result.metallicAxis=float(down(GLFW_KEY_X))-float(down(GLFW_KEY_Z));
    result.roughnessAxis=float(down(GLFW_KEY_V))-float(down(GLFW_KEY_C));
    glfwGetCursorPos(window,&result.mouseX,&result.mouseY);
    return result;
}

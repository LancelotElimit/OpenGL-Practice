#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool valid() const;
    GLFWwindow* get() const;
    int gladVersion() const;

    bool shouldClose() const;
    void framebufferSize(int& width, int& height) const;
    float aspectRatio() const;
    void swapBuffers() const;
    void pollEvents() const;

private:
    GLFWwindow* window_ = nullptr;
    int gladVersion_ = 0;
};

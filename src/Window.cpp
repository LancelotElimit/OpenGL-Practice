#include "Window.h"

#include <iostream>

namespace {

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    if (width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}

} // namespace

Window::Window(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create the GLFW window.\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window_);
    gladVersion_ = gladLoadGL(glfwGetProcAddress);
    if (gladVersion_ == 0) {
        std::cerr << "Failed to load OpenGL functions through GLAD.\n";
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return;
    }

    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(
        window_,
        &framebufferWidth,
        &framebufferHeight
    );
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::valid() const {
    return window_ != nullptr && gladVersion_ != 0;
}

GLFWwindow* Window::get() const {
    return window_;
}

int Window::gladVersion() const {
    return gladVersion_;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

float Window::aspectRatio() const {
    int width = 0;
    int height = 0;
    framebufferSize(width, height);
    return height > 0
        ? static_cast<float>(width) / static_cast<float>(height)
        : 1.0f;
}

void Window::swapBuffers() const {
    glfwSwapBuffers(window_);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

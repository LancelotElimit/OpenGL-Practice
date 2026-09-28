#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "AssetPaths.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "PbrMaterial.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture2D.h"
#include "TextureCache.h"
#include "Window.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>

int main() {
    Window applicationWindow(1280, 720, "OpenGL Practice");
    if (!applicationWindow.valid()) {
        return 1;
    }
    GLFWwindow* window = applicationWindow.get();

    const int version = applicationWindow.gladVersion();
    std::cout << "OpenGL version: "
              << GLAD_VERSION_MAJOR(version)
              << '.'
              << GLAD_VERSION_MINOR(version)
              << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    const std::filesystem::path shaderDirectory = findAssetPath("shaders");
    const std::filesystem::path modelPath = findAssetPath("assets/cube.obj");
    const std::filesystem::path texturePath = findAssetPath(
        "assets/basi6a08.png"
    );
    const std::filesystem::path environmentPath = findAssetPath(
        "assets/sunset_jhbcentral_1k.hdr"
    );
    const std::filesystem::path animatedModelPath = findAssetPath(
        "assets/SimpleSkin.gltf"
    );
    if (shaderDirectory.empty() || modelPath.empty() || texturePath.empty()) {
        std::cerr << "Could not find the shaders or required assets.\n";
        return 1;
    }

    Model model;
    if (!model.load(modelPath)) {
        return 1;
    }
    std::cout << "Model geometry: "
              << model.vertices().size() / Model::VertexStrideFloats
              << " unique vertices, "
              << model.indices().size()
              << " indices\n";

    Mesh modelMesh;
    modelMesh.uploadIndexed(
        model.vertices().data(),
        model.vertices().size(),
        Model::VertexStrideFloats,
        {
            {0, 3, 0},
            {1, 2, 3},
            {2, 3, 5},
            {3, 4, 8}
        },
        model.indices().data(),
        model.indices().size()
    );

    const float floorVertices[] = {
        // position             UV          normal           tangent + sign
        -5, -.5f, -5,  0, 0,   0, 1, 0,    1, 0, 0, -1,
         5, -.5f, -5,  5, 0,   0, 1, 0,    1, 0, 0, -1,
         5, -.5f,  5,  5, 5,   0, 1, 0,    1, 0, 0, -1,
         5, -.5f,  5,  5, 5,   0, 1, 0,    1, 0, 0, -1,
        -5, -.5f,  5,  0, 5,   0, 1, 0,    1, 0, 0, -1,
        -5, -.5f, -5,  0, 0,   0, 1, 0,    1, 0, 0, -1
    };
    Mesh floorMesh;
    floorMesh.upload(
        floorVertices,
        sizeof(floorVertices) / sizeof(float),
        12,
        {
            {0, 3, 0},
            {1, 2, 3},
            {2, 3, 5},
            {3, 4, 8}
        }
    );

    const float debugQuadVertices[] = {
        // position   UV
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f
    };
    Mesh debugMesh;
    debugMesh.upload(
        debugQuadVertices,
        sizeof(debugQuadVertices) / sizeof(float),
        4,
        {
            {0, 2, 0},
            {1, 2, 2}
        }
    );

    Texture2D fallbackTexture;
    if (!fallbackTexture.loadRGBA(texturePath, true)) {
        return 1;
    }

    TextureCache textureCache;
    MaterialLibrary materialLibrary;
    materialLibrary.loadForModel(
        model,
        modelPath.parent_path(),
        textureCache
    );

    PbrMaterial floorMaterial;
    floorMaterial.load(
        findAssetPath("assets/concrete_diff_1k.jpg"),
        findAssetPath("assets/concrete_nor_gl_1k.jpg"),
        findAssetPath("assets/concrete_rough_1k.jpg"),
        findAssetPath("assets/concrete_ao_1k.jpg"),
        textureCache
    );
    std::cout << "Shared texture cache: "
              << textureCache.size()
              << " unique file textures\n";

    const unsigned char normalPixels[] = {
        128, 128, 255, 255,    205, 128, 220, 255,
        128, 205, 220, 255,     75,  75, 205, 255
    };
    Texture2D normalTexture;
    normalTexture.createRGBA(2, 2, normalPixels);

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    Scene scene;
    Renderer renderer(
        shaderDirectory,
        environmentPath,
        animatedModelPath
    );
    if (!renderer.valid()) {
        return 1;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    float lastFrameTime = 0.0f;
    float exposure = 1.0f;
    float modelMetallic = 0.35f;
    float modelRoughness = 0.28f;
    bool bloomEnabled = true;
    bool f3WasPressed = false;

    while (!applicationWindow.shouldClose()) {
        const float currentFrameTime = static_cast<float>(glfwGetTime());
        const float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const bool f3IsPressed =
            glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
        if (f3IsPressed && !f3WasPressed) {
            bloomEnabled = !bloomEnabled;
        }
        f3WasPressed = f3IsPressed;

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            exposure += deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            exposure -= deltaTime;
        }
        exposure = std::clamp(exposure, 0.1f, 5.0f);

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
            modelMetallic -= deltaTime * 0.5f;
        }
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
            modelMetallic += deltaTime * 0.5f;
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            modelRoughness -= deltaTime * 0.5f;
        }
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
            modelRoughness += deltaTime * 0.5f;
        }
        modelMetallic = std::clamp(modelMetallic, 0.0f, 1.0f);
        modelRoughness = std::clamp(modelRoughness, 0.05f, 1.0f);

        camera.processKeyboard(window, deltaTime);
        camera.processMouse(window);
        scene.update(currentFrameTime);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        applicationWindow.framebufferSize(
            framebufferWidth,
            framebufferHeight
        );
        if (framebufferWidth == 0 || framebufferHeight == 0) {
            applicationWindow.pollEvents();
            continue;
        }

        renderer.render(
            scene,
            camera,
            framebufferWidth,
            framebufferHeight,
            modelMesh,
            floorMesh,
            debugMesh,
            model,
            fallbackTexture,
            normalTexture,
            materialLibrary,
            floorMaterial,
            glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS,
            bloomEnabled,
            exposure,
            modelMetallic,
            modelRoughness,
            currentFrameTime
        );

        applicationWindow.swapBuffers();
        applicationWindow.pollEvents();
    }

    renderer.destroy();
    modelMesh.destroy();
    floorMesh.destroy();
    debugMesh.destroy();
    fallbackTexture.destroy();
    normalTexture.destroy();
    materialLibrary.destroy();
    floorMaterial.destroy();
    textureCache.destroy();
    return 0;
}

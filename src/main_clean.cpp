#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "AssetPaths.h"
#include "Camera.h"
#include "DebugPanel.h"
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
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

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
    const std::filesystem::path modelPath = findAssetPath(
        "assets/my_model/Mouse-5079f33c/obj/mouse_5.obj"
    );
    const std::filesystem::path environmentPath = findAssetPath(
        "assets/sunset_jhbcentral_1k.hdr"
    );
    const std::filesystem::path animatedModelPath = findAssetPath(
        "assets/SimpleSkin.gltf"
    );
    if (shaderDirectory.empty() || modelPath.empty()) {
        std::cerr << "Could not find the shaders or required assets.\n";
        return 1;
    }

    Model model;
    // The exported OBJ also contains a huge decorative Plane object.
    if (!model.load(modelPath, "Plane")) {
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
    const unsigned char whitePixel[] = {255, 255, 255, 255};
    fallbackTexture.createRGBA(1, 1, whitePixel);

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

    const unsigned char normalPixels[] = {128, 128, 255, 255};
    Texture2D normalTexture;
    normalTexture.createRGBA(1, 1, normalPixels);

    Camera camera(glm::vec3(0.0f, 1.5f, 4.0f), -90.0f, -20.0f);
    Scene scene;
    const float targetModelRadius = 1.7f;
    const float modelScale = targetModelRadius
        / std::max(model.boundsRadius(), 0.001f);
    float lowestModelY = model.boundsCenter().y;
    for (std::size_t index = 1; index < model.vertices().size();
         index += Model::VertexStrideFloats) {
        lowestModelY = std::min(lowestModelY, model.vertices()[index]);
    }
    const glm::vec3 modelOffset(
        -model.boundsCenter().x * modelScale,
        -0.5f - lowestModelY * modelScale,
        -model.boundsCenter().z * modelScale
    );
    const glm::mat4 modelImportTransform =
        glm::translate(glm::mat4(1.0f), modelOffset)
        * glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
    scene.setModelImportTransform(modelImportTransform);
    Renderer renderer(
        shaderDirectory,
        environmentPath,
        animatedModelPath
    );
    if (!renderer.valid()) {
        return 1;
    }
    renderer.setShowOnlyImportedModel(false);
    const std::filesystem::path sampleGltf = findAssetPath("assets/BoxTextured.glb");
    if (!sampleGltf.empty() && renderer.gltfScene().load(sampleGltf)) {
        renderer.showGltfScene() = true;
    }

    DebugPanel debugPanel(window);
    if (!debugPanel.valid()) {
        std::cerr << "Could not initialize the diagnostics panel.\n";
        return 1;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    float lastFrameTime = static_cast<float>(glfwGetTime());
    float exposure = 1.0f;
    float modelMetallic = 0.35f;
    float modelRoughness = 0.28f;
    bool bloomEnabled = true;
    bool f3WasPressed = false;
    bool f4WasPressed = false;
    bool uiInteractive = true;
    float statsUpdateTime = lastFrameTime;
    float displayedFps = 0.0f;
    unsigned int statsFrameCount = 0;

    while (!applicationWindow.shouldClose()) {
        const float currentFrameTime = static_cast<float>(glfwGetTime());
        const float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const bool f4IsPressed =
            glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
        if (f4IsPressed && !f4WasPressed) {
            uiInteractive = !uiInteractive;
            glfwSetInputMode(
                window,
                GLFW_CURSOR,
                uiInteractive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED
            );
            camera.resetMouseSample();
        }
        f4WasPressed = f4IsPressed;

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

        const EditorViewportSize viewport = debugPanel.beginFrame(renderer, uiInteractive);
        if ((!uiInteractive || debugPanel.sceneNavigating())
            && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE) {
            camera.processKeyboard(window, deltaTime);
            camera.processMouse(window);
        } else {
            camera.resetMouseSample();
        }
        scene.update(currentFrameTime);

        renderer.render(
            scene,
            camera,
            viewport.width,
            viewport.height,
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
        ++statsFrameCount;

        if (currentFrameTime - statsUpdateTime >= 0.25f) {
            const float statsElapsed = currentFrameTime - statsUpdateTime;
            displayedFps = statsElapsed > 0.0f
                ? static_cast<float>(statsFrameCount) / statsElapsed
                : 0.0f;
            statsUpdateTime = currentFrameTime;
            statsFrameCount = 0;
            const RendererStats& stats = renderer.stats();
            std::ostringstream title;
            title << std::fixed << std::setprecision(1)
                  << "OpenGL Practice | FPS " << displayedFps
                  << " | Draws " << stats.drawCalls
                  << " | Triangles " << stats.submittedTriangles
                  << " | Instances " << stats.visibleInstances
                  << '/' << stats.totalInstances
                  << " | Cached textures " << textureCache.size()
                  << " | Fluid " << renderer.fluidSystem().simulationMilliseconds()
                  << "/" << renderer.fluidSystem().surfaceMilliseconds() << "ms"
                  << " | Metal " << modelMetallic
                  << " | Rough " << modelRoughness;
            applicationWindow.setTitle(title.str());
        }

        debugPanel.draw(
            renderer,
            camera,
            scene,
            renderer.stats(),
            displayedFps,
            deltaTime,
            textureCache.size(),
            uiInteractive,
            modelMetallic,
            modelRoughness,
            exposure,
            bloomEnabled
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

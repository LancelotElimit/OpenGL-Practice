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
#include "Project.h"
#include "EngineApplication.h"
#include "PlaySession.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

int EngineApplication::run(Project& project,const ScriptRegistry& scripts) {
    setProjectAssetRoot(project.assetRoot());
    Window applicationWindow(1440, 900, project.name().c_str());
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
    const auto modelPath=project.asset("model");
    const auto environmentPath=project.asset("environment");
    const auto animatedModelPath=project.asset("animated");
    if (shaderDirectory.empty()) {
        std::cerr << "Could not find the shaders or required assets.\n";
        return 1;
    }

    Model model;
    // The exported OBJ also contains a huge decorative Plane object.
    if (!modelPath.empty() && !model.load(modelPath,project.excludedObject())) {
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
        -5, 0, -5,  0, 0,   0, 1, 0,    1, 0, 0, -1,
         5, 0, -5,  5, 0,   0, 1, 0,    1, 0, 0, -1,
         5, 0,  5,  5, 5,   0, 1, 0,    1, 0, 0, -1,
         5, 0,  5,  5, 5,   0, 1, 0,    1, 0, 0, -1,
        -5, 0,  5,  0, 5,   0, 1, 0,    1, 0, 0, -1,
        -5, 0, -5,  0, 0,   0, 1, 0,    1, 0, 0, -1
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
        project.asset("floorColor"), project.asset("floorNormal"),
        project.asset("floorRoughness"), project.asset("floorAO"),
        textureCache
    );
    std::cout << "Shared texture cache: "
              << textureCache.size()
              << " unique file textures\n";

    const unsigned char normalPixels[] = {128, 128, 255, 255};
    Texture2D normalTexture;
    normalTexture.createRGBA(1, 1, normalPixels);

    Camera camera(glm::vec3(0.0f, 1.5f, 4.0f), -90.0f, -20.0f);
    Scene editingScene;
    Scene& scene=editingScene;
    const float targetModelRadius = project.modelRadius();
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
    const glm::vec3 objPivot = glm::vec3(modelImportTransform * glm::vec4(model.boundsCenter(), 1));
    scene.configureObject(SceneObjectKind::Obj,
        glm::translate(glm::mat4(1), -objPivot) * modelImportTransform,
        model.boundsCenter(), model.boundsRadius());
    std::vector<glm::vec3> objPickTriangles;
    objPickTriangles.reserve(model.indices().size());
    for (const auto index : model.indices()) {
        const auto offset = static_cast<std::size_t>(index) * Model::VertexStrideFloats;
        objPickTriangles.emplace_back(model.vertices()[offset], model.vertices()[offset+1], model.vertices()[offset+2]);
    }
    scene.setPickingTriangles(SceneObjectKind::Obj, std::move(objPickTriangles));
    Renderer renderer(
        shaderDirectory,
        environmentPath,
        animatedModelPath
    );
    if (!renderer.valid()) {
        return 1;
    }
    renderer.setShowOnlyImportedModel(false);
    const auto sampleGltf=project.asset("gltf");
    if (!sampleGltf.empty() && renderer.gltfScene().load(sampleGltf)) {
        renderer.showGltfScene() = true;
    }
    const float gltfScale = .75f / std::max(renderer.gltfScene().radius(), .001f);
    scene.configureObject(SceneObjectKind::Gltf,
        glm::scale(glm::mat4(1), glm::vec3(gltfScale))
        * glm::translate(glm::mat4(1), -renderer.gltfScene().center()),
        renderer.gltfScene().center(), renderer.gltfScene().radius());
    scene.configureObject(SceneObjectKind::Skinned,
        glm::scale(glm::mat4(1), glm::vec3(.75f))
        * glm::translate(glm::mat4(1), -renderer.gltfModel().boundsCenter()),
        renderer.gltfModel().boundsCenter(), renderer.gltfModel().boundsRadius());
    scene.setPickingTriangles(SceneObjectKind::Gltf, renderer.gltfScene().pickingTriangles());

    if (!project.loadScene(scene)) { std::cerr << project.error() << '\n'; return 1; }
    renderer.showGltfModel() = true;
    PlaySession play(scripts);
    DebugPanel debugPanel(window, project, play);
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
    float simulationTime=0;
    std::string lastScriptError;

    while (!applicationWindow.shouldClose() && project.requestedOpen.empty()) {
        const float currentFrameTime = static_cast<float>(glfwGetTime());
        const float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Apply toolbar commands before taking references to the active scene.
        const auto command=play.command; play.command=PlayCommand::None;
        if(command==PlayCommand::Start) {
            if(play.start(editingScene,camera)) {
                renderer.resetSimulation(); simulationTime=0; uiInteractive=true;
                glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
                debugPanel.runtimeMessage("Play: runtime copy started. Click Scene View for WASD.");
            } else debugPanel.runtimeMessage(play.error());
        }
        if(command==PlayCommand::Pause) play.pause();
        if(command==PlayCommand::Resume) play.resume();
        if(command==PlayCommand::Stop) {
            play.stop(camera); renderer.resetSimulation(); simulationTime=0;
            debugPanel.runtimeMessage("Stopped. Editing scene and camera restored.");
        }
        Scene& scene=play.activeScene(editingScene);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if(play.active()) play.pause();
            else if(!ImGui::GetIO().WantTextInput) glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const bool f4IsPressed =
            glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
        if (f4IsPressed && !f4WasPressed && !play.active()) {
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

        const bool materialShortcuts = !play.active() && !ImGui::GetIO().WantCaptureKeyboard;
        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            exposure += deltaTime;
        }
        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            exposure -= deltaTime;
        }
        exposure = std::clamp(exposure, 0.1f, 5.0f);

        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
            modelMetallic -= deltaTime * 0.5f;
        }
        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
            modelMetallic += deltaTime * 0.5f;
        }
        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            modelRoughness -= deltaTime * 0.5f;
        }
        if (materialShortcuts && glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
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

        const EditorViewportSize viewport = debugPanel.beginFrame(renderer, scene, uiInteractive);
        if (!play.active() && (!uiInteractive || debugPanel.sceneNavigating())
            && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE) {
            camera.processKeyboard(window, deltaTime);
            camera.processMouse(window);
        } else {
            camera.resetMouseSample();
        }
        GameInput input;
        if(play.active() && debugPanel.gameInputFocused() && glfwGetWindowAttrib(window,GLFW_FOCUSED)) {
            // ImGui's event queue retains quick taps even if GLFW has already
            // received both press and release in a single pollEvents call.
            const auto down=[](ImGuiKey key) {
                return ImGui::IsKeyDown(key) || ImGui::IsKeyPressed(key,false);
            };
            input.movement.x=(down(ImGuiKey_D)?1.f:0.f)-(down(ImGuiKey_A)?1.f:0.f);
            input.movement.y=(down(ImGuiKey_W)?1.f:0.f)-(down(ImGuiKey_S)?1.f:0.f);
            if(ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyAlt) input={};
        }
        play.update(deltaTime,input,camera);
        if(play.active()) simulationTime=play.time();
        else simulationTime+=std::clamp(deltaTime,0.f,.05f);
        if(!play.error().empty() && play.error()!=lastScriptError) {
            debugPanel.runtimeMessage(play.error()); lastScriptError=play.error();
        }
        scene.update(simulationTime);

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
            simulationTime, play.state()!=PlayState::Paused
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
                  << project.name() << " | FPS " << displayedFps
                  << " | Draws " << stats.drawCalls
                  << " | Triangles " << stats.submittedTriangles
                  << " | Instances " << stats.visibleInstances
                  << '/' << stats.totalInstances
                  << " | Cached textures " << textureCache.size()
                  << " | Fluid " << renderer.stats().fluidUpdateMs << "ms"
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

    play.stop(camera);
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

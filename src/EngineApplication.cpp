#include "EngineApplication.h"
#include "AssetPaths.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "PbrMaterial.h"
#include "Project.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture2D.h"
#include "TextureCache.h"
#include "Window.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

struct EngineApplication::State {
    // Context is destroyed last. Renderer/materials are destroyed before cache.
    Window window;
    TextureCache textureCache;
    Model model;
    Mesh modelMesh, floorMesh, debugMesh;
    Texture2D fallbackTexture, normalTexture;
    MaterialLibrary materialLibrary;
    PbrMaterial floorMaterial;
    std::unique_ptr<AssetLibrary> assets;
    std::unique_ptr<SimulationRuntime> simulations;
    std::unique_ptr<Renderer> renderer;
    Scene scene;
    Camera camera{glm::vec3(0, 1.5f, 4), -90, -20};
    explicit State(Project &project, bool visible)
        : window(1440, 900, project.name().c_str(), visible) {}
    bool load(Project &project) {
        const int version = window.gladVersion();
        std::cout << "OpenGL version: " << GLAD_VERSION_MAJOR(version) << '.'
                  << GLAD_VERSION_MINOR(version) << '\n';
        std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

        const std::filesystem::path shaderDirectory = findAssetPath("shaders");
        const auto modelPath = project.asset("model");
        const auto environmentPath = project.asset("environment");
        const auto animatedModelPath = project.asset("animated");
        if (shaderDirectory.empty()) {
            std::cerr << "Could not find the shaders or required assets.\n";
            return false;
        }

        // The exported OBJ also contains a huge decorative Plane object.
        if (!modelPath.empty() && !model.load(modelPath, project.excludedObject())) {
            return false;
        }
        std::cout << "Model geometry: " << model.vertices().size() / Model::VertexStrideFloats
                  << " unique vertices, " << model.indices().size() << " indices\n";

        modelMesh.uploadIndexed(model.vertices().data(), model.vertices().size(),
                                Model::VertexStrideFloats,
                                {{0, 3, 0}, {1, 2, 3}, {2, 3, 5}, {3, 4, 8}},
                                model.indices().data(), model.indices().size());

        const float floorVertices[] = {
            // position             UV          normal           tangent + sign
            -5, 0, -5, 0, 0, 0, 1, 0, 1, 0, 0, -1, 5,  0, -5, 5, 0, 0, 1, 0, 1, 0, 0, -1,
            5,  0, 5,  5, 5, 0, 1, 0, 1, 0, 0, -1, 5,  0, 5,  5, 5, 0, 1, 0, 1, 0, 0, -1,
            -5, 0, 5,  0, 5, 0, 1, 0, 1, 0, 0, -1, -5, 0, -5, 0, 0, 0, 1, 0, 1, 0, 0, -1};
        floorMesh.upload(floorVertices, sizeof(floorVertices) / sizeof(float), 12,
                         {{0, 3, 0}, {1, 2, 3}, {2, 3, 5}, {3, 4, 8}});

        const float debugQuadVertices[] = {// position   UV
                                           -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 0.0f,
                                           1.0f,  1.0f,  1.0f, 1.0f, 1.0f,  1.0f,  1.0f, 1.0f,
                                           -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f};
        debugMesh.upload(debugQuadVertices, sizeof(debugQuadVertices) / sizeof(float), 4,
                         {{0, 2, 0}, {1, 2, 2}});

        const unsigned char whitePixel[] = {255, 255, 255, 255};
        fallbackTexture.createRGBA(1, 1, whitePixel);

        materialLibrary.loadForModel(model, modelPath.parent_path(), textureCache);

        floorMaterial.load(project.asset("floorColor"), project.asset("floorNormal"),
                           project.asset("floorRoughness"), project.asset("floorAO"), textureCache);
        std::cout << "Shared texture cache: " << textureCache.size() << " unique file textures\n";

        const unsigned char normalPixels[] = {128, 128, 255, 255};
        normalTexture.createRGBA(1, 1, normalPixels);

        const float targetModelRadius = project.modelRadius();
        const float modelScale = targetModelRadius / std::max(model.boundsRadius(), 0.001f);
        float lowestModelY = model.boundsCenter().y;
        for (std::size_t index = 1; index < model.vertices().size();
             index += Model::VertexStrideFloats) {
            lowestModelY = std::min(lowestModelY, model.vertices()[index]);
        }
        const glm::vec3 modelOffset(-model.boundsCenter().x * modelScale,
                                    -0.5f - lowestModelY * modelScale,
                                    -model.boundsCenter().z * modelScale);
        const glm::mat4 modelImportTransform = glm::translate(glm::mat4(1.0f), modelOffset) *
                                               glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
        scene.setModelImportTransform(modelImportTransform);
        const glm::vec3 objPivot =
            glm::vec3(modelImportTransform * glm::vec4(model.boundsCenter(), 1));
        scene.configureObject(SceneObjectKind::Obj,
                              glm::translate(glm::mat4(1), -objPivot) * modelImportTransform,
                              model.boundsCenter(), model.boundsRadius());
        std::vector<glm::vec3> objPickTriangles;
        objPickTriangles.reserve(model.indices().size());
        for (const auto index : model.indices()) {
            const auto offset = static_cast<std::size_t>(index) * Model::VertexStrideFloats;
            objPickTriangles.emplace_back(model.vertices()[offset], model.vertices()[offset + 1],
                                          model.vertices()[offset + 2]);
        }
        scene.setPickingTriangles(SceneObjectKind::Obj, std::move(objPickTriangles));
        assets = std::make_unique<AssetLibrary>(shaderDirectory, project.assetRoot(),
                                                animatedModelPath, textureCache);
        simulations = std::make_unique<SimulationRuntime>(shaderDirectory);
        renderer =
            std::make_unique<Renderer>(shaderDirectory, environmentPath, *simulations, *assets);
        if (!renderer->valid()) {
            return false;
        }
        renderer->setShowOnlyImportedModel(false);
        const auto sampleGltf = project.asset("gltf");
        if (!sampleGltf.empty() && renderer->gltfScene().load(sampleGltf)) {
            renderer->showGltfScene() = true;
        }
        const float gltfScale = .75f / std::max(renderer->gltfScene().radius(), .001f);
        scene.configureObject(SceneObjectKind::Gltf,
                              glm::scale(glm::mat4(1), glm::vec3(gltfScale)) *
                                  glm::translate(glm::mat4(1), -renderer->gltfScene().center()),
                              renderer->gltfScene().center(), renderer->gltfScene().radius());
        scene.setPickingTriangles(SceneObjectKind::Gltf, renderer->gltfScene().pickingTriangles());

        if (!project.loadScene(scene)) {
            std::cerr << project.error() << '\n';
            return false;
        }
        assets->prepare(scene);
        renderer->showGltfModel() = true;

        return true;
    }
};
EngineApplication::EngineApplication() = default;
EngineApplication::~EngineApplication() = default;
bool EngineApplication::initialize(Project &project, bool visible) {
    state_.reset();
    setProjectAssetRoot(project.assetRoot());
    auto candidate = std::make_unique<State>(project, visible);
    if (!candidate->window.valid() || !candidate->load(project))
        return false;
    state_ = std::move(candidate);
    return true;
}
Window &EngineApplication::window() { return state_->window; }
Scene &EngineApplication::scene() { return state_->scene; }
Camera &EngineApplication::camera() { return state_->camera; }
Renderer &EngineApplication::renderer() { return *state_->renderer; }
std::size_t EngineApplication::textureCount() const { return state_->textureCache.size(); }
void EngineApplication::update(Scene &scene, float delta, float time, bool advance) {
    state_->assets->prepare(scene);
    scene.update(time);
    state_->simulations->update(scene, delta, time, advance, *state_->assets);
    for (const auto &item : scene.objects())
        if (item.kind == SceneObjectKind::Skinned) {
            auto *animation = state_->simulations->animation(item.id);
            if (!animation || !animation->valid())
                continue;
            auto *object = scene.find(item.id);
            object->boundsCenter = animation->boundsCenter();
            object->boundsRadius = animation->boundsRadius();
            object->importTransform = glm::scale(glm::mat4(1), glm::vec3(.75f)) *
                                      glm::translate(glm::mat4(1), -animation->boundsCenter());
        }
}
void EngineApplication::render(Scene &scene, Camera &camera, int width, int height,
                               const RenderOptions &options, float time) {
    auto &s = *state_;
    s.renderer->render(scene, camera, width, height, s.modelMesh, s.floorMesh, s.debugMesh, s.model,
                       s.fallbackTexture, s.normalTexture, s.materialLibrary, s.floorMaterial,
                       options.shadowPreview, options.grayscale, options.bloom, options.exposure,
                       options.metallic, options.roughness, time);
}
void EngineApplication::present(int width, int height) {
    // Player host displays the off-screen final color without any UI backend.
    GLuint readFramebuffer = 0;
    glGenFramebuffers(1, &readFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           state_->renderer->viewportTexture(), 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &readFramebuffer);
}

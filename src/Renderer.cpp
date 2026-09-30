#include "Renderer.h"

#include "Camera.h"
#include "Frustum.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "PbrMaterial.h"
#include "Scene.h"
#include "Texture2D.h"

#include <algorithm>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace {

constexpr GLsizei shadowMapSize = 1024;
constexpr GLsizei pointShadowMapSize = 1024;
constexpr float pointShadowFarPlane = 25.0f;

float maximumScale(const glm::mat4 &transform) {
    return std::max({glm::length(glm::vec3(transform[0])), glm::length(glm::vec3(transform[1])),
                     glm::length(glm::vec3(transform[2]))});
}

glm::vec3 transformedCenter(const glm::mat4 &transform, const glm::vec3 &localCenter) {
    return glm::vec3(transform * glm::vec4(localCenter, 1.0f));
}

} // namespace

Renderer::Renderer(const std::filesystem::path &shaderDirectory,
                   const std::filesystem::path &environmentPath, SimulationRuntime &simulations,
                   AssetLibrary &assets)
    : mainProgram_(shaderDirectory / "main.vert", shaderDirectory / "main.frag", "Main shader"),
      lightProgram_(shaderDirectory / "light_marker.vert", shaderDirectory / "light_marker.frag",
                    "Light marker shader"),
      depthProgram_(shaderDirectory / "depth.vert", shaderDirectory / "depth.frag", "Depth shader"),
      debugProgram_(shaderDirectory / "debug.vert", shaderDirectory / "debug.frag", "Debug shader"),
      brightPassProgram_(shaderDirectory / "postprocess.vert", shaderDirectory / "bright_pass.frag",
                         "Bright-pass shader"),
      blurProgram_(shaderDirectory / "postprocess.vert", shaderDirectory / "blur.frag",
                   "Gaussian blur shader"),
      postprocessProgram_(shaderDirectory / "postprocess.vert",
                          shaderDirectory / "postprocess.frag", "Post-process shader"),
      environmentIbl_(shaderDirectory, environmentPath), simulations_(simulations),
      assets_(assets) {
    if (!mainProgram_.valid() || !lightProgram_.valid() || !depthProgram_.valid() ||
        !debugProgram_.valid() || !brightPassProgram_.valid() || !blurProgram_.valid() ||
        !postprocessProgram_.valid() || !environmentIbl_.valid()) {
        return;
    }

    transformLocation_ = mainProgram_.uniform("uViewProjection");
    modelLocation_ = mainProgram_.uniform("uModel");
    useInstancingLocation_ = mainProgram_.uniform("uUseInstancing");
    materialColorLocation_ = mainProgram_.uniform("uMaterialColor");
    metallicLocation_ = mainProgram_.uniform("uMetallic");
    roughnessLocation_ = mainProgram_.uniform("uRoughness");
    aoLocation_ = mainProgram_.uniform("uAo");
    usePbrMapsLocation_ = mainProgram_.uniform("uUsePbrMaps");
    textureScaleLocation_ = mainProgram_.uniform("uTextureScale");
    lightPositionLocation_ = mainProgram_.uniform("uLightPosition");
    lightPosition2Location_ = mainProgram_.uniform("uLightPosition2");
    spotLightPositionLocation_ = mainProgram_.uniform("uSpotLightPosition");
    spotLightDirectionLocation_ = mainProgram_.uniform("uSpotLightDirection");
    cameraPositionLocation_ = mainProgram_.uniform("uCameraPosition");
    lightSpaceMatrixLocation_ = mainProgram_.uniform("uLightSpaceMatrix");
    lightTransformLocation_ = lightProgram_.uniform("uViewProjection");
    markerLightPositionLocation_ = lightProgram_.uniform("uLightPosition");
    markerColorLocation_ = lightProgram_.uniform("uMarkerColor");
    depthLightSpaceLocation_ = depthProgram_.uniform("uLightSpaceMatrix");
    depthModelLocation_ = depthProgram_.uniform("uModel");
    depthUseInstancingLocation_ = depthProgram_.uniform("uUseInstancing");
    grayscaleLocation_ = postprocessProgram_.uniform("uGrayscale");
    bloomEnabledLocation_ = postprocessProgram_.uniform("uBloomEnabled");
    exposureLocation_ = postprocessProgram_.uniform("uExposure");
    blurHorizontalLocation_ = blurProgram_.uniform("uHorizontal");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    if (!spotlightShadowMap_.create(shadowMapSize) || !pointShadowMap_.create(pointShadowMapSize)) {
        return;
    }

    configureStaticUniforms();
    valid_ = true;
}

Renderer::~Renderer() { destroy(); }

bool Renderer::valid() const { return valid_; }

const RendererStats &Renderer::stats() const { return stats_; }

GLuint Renderer::viewportTexture() const { return viewportTarget_.colorTexture(); }

void Renderer::setShowOnlyImportedModel(bool enabled) { showOnlyImportedModel_ = enabled; }

void Renderer::resetParticleEmitter(std::uint32_t id) { simulations_.resetEmitter(id); }
bool &Renderer::showGltfModel() { return showAnimatedModel_; }
GltfScene &Renderer::gltfScene() { return assets_.legacyGltf(); }
bool &Renderer::showGltfScene() { return showGltfScene_; }
FluidSystem &Renderer::fluidSystem(std::uint32_t id) { return simulations_.fluidSystem(id); }
Fluid2D &Renderer::smoke2D(std::uint32_t id) { return simulations_.smoke2D(id); }

void Renderer::configureStaticUniforms() {
    mainProgram_.use();
    glUniform1i(mainProgram_.uniform("uTexture"), 0);
    glUniform1i(mainProgram_.uniform("uShadowMap"), 1);
    glUniform1i(mainProgram_.uniform("uPointShadowMap"), 2);
    glUniform1i(mainProgram_.uniform("uNormalMap"), 3);
    glUniform1i(mainProgram_.uniform("uIrradianceMap"), 4);
    glUniform1i(mainProgram_.uniform("uPrefilterMap"), 5);
    glUniform1i(mainProgram_.uniform("uBrdfLut"), 6);
    glUniform1i(mainProgram_.uniform("uRoughnessMap"), 7);
    glUniform1i(mainProgram_.uniform("uAoMap"), 8);
    glUniform1f(mainProgram_.uniform("uPointShadowFarPlane"), pointShadowFarPlane);
    glUniform3f(mainProgram_.uniform("uLightColor"), 3.0f, 2.7f, 2.2f);
    glUniform3f(mainProgram_.uniform("uLightColor2"), 0.5f, 1.0f, 3.0f);
    glUniform3f(mainProgram_.uniform("uSpotLightColor"), 2.0f, 2.0f, 2.0f);
    glUniform1f(mainProgram_.uniform("uSpotInnerCutoff"), glm::cos(glm::radians(12.5f)));
    glUniform1f(mainProgram_.uniform("uSpotOuterCutoff"), glm::cos(glm::radians(17.5f)));
    debugProgram_.use();
    glUniform1i(debugProgram_.uniform("uDepthMap"), 0);

    brightPassProgram_.use();
    glUniform1i(brightPassProgram_.uniform("uSceneTexture"), 0);
    glUniform1f(brightPassProgram_.uniform("uThreshold"), 1.0f);

    blurProgram_.use();
    glUniform1i(blurProgram_.uniform("uImage"), 0);

    postprocessProgram_.use();
    glUniform1i(postprocessProgram_.uniform("uSceneTexture"), 0);
    glUniform1i(postprocessProgram_.uniform("uBloomTexture"), 1);
}

void Renderer::render(const Scene &scene, const Camera &camera, int framebufferWidth,
                      int framebufferHeight, Mesh &modelMesh, const Mesh &floorMesh,
                      const Mesh &debugMesh, const Model &model, const Texture2D &fallbackTexture,
                      const Texture2D &normalTexture, const MaterialLibrary &materialLibrary,
                      const PbrMaterial &floorMaterial, bool showShadowMap, bool useGrayscale,
                      bool bloomEnabled, float exposure, float modelMetallic, float modelRoughness,
                      float timeSeconds) {
    stats_ = {};
    const auto &modelTransforms = scene.modelTransforms();
    const glm::vec3 &lightPosition = scene.primaryLightPosition();
    const glm::vec3 &lightPosition2 = scene.secondaryLightPosition();

    const glm::mat4 view = camera.viewMatrix();
    const glm::mat4 projection = glm::perspective(
        glm::radians(camera.fieldOfView()),
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);
    const glm::mat4 viewProjection = projection * view;
    const Frustum frustum(viewProjection);

    static_cast<SimulationStats &>(stats_) = simulations_.stats();
    stats_.drawCalls += stats_.smokePasses;
    stats_.submittedTriangles += 2 * stats_.smokePasses;

    struct ModelBatch {
        Mesh *mesh;
        const Model *model;
        const MaterialLibrary *materials;
        std::vector<glm::mat4> all, visible;
        float metallic, roughness;
    };
    std::vector<ModelBatch> batches;
    batches.push_back(
        {&modelMesh, &model, &materialLibrary, modelTransforms, {}, modelMetallic, modelRoughness});
    for (const auto &object : scene.objects()) {
        if (object.kind != SceneObjectKind::Obj || !object.enabledInHierarchy())
            continue;
        const auto &component = dynamic_cast<const ModelObject &>(object).model;
        if (component.asset.source.empty() && !component.overrideMaterial)
            continue;
        auto *asset = component.asset.source.empty() ? nullptr : assets_.obj(component.asset);
        if (!component.asset.source.empty() && !asset)
            continue;
        batches.push_back({asset ? &asset->mesh : &modelMesh,
                           asset ? &asset->model : &model,
                           asset ? &asset->materials : &materialLibrary,
                           {object.matrix()},
                           {},
                           component.overrideMaterial ? component.metallic : modelMetallic,
                           component.overrideMaterial ? component.roughness : modelRoughness});
    }
    for (auto &batch : batches) {
        stats_.totalInstances += batch.all.size();
        for (const auto &transform : batch.all)
            if (frustum.containsSphere(transformedCenter(transform, batch.model->boundsCenter()),
                                       batch.model->boundsRadius() * maximumScale(transform)))
                batch.visible.push_back(transform);
        stats_.visibleInstances += batch.visible.size();
    }
    const auto drawObjShadows = [&] {
        for (auto &batch : batches) {
            if (batch.all.empty())
                continue;
            batch.mesh->updateInstanceTransforms(batch.all.data(), batch.all.size());
            batch.mesh->drawInstanced();
            ++stats_.drawCalls;
            stats_.submittedTriangles += batch.model->indices().size() / 3 * batch.all.size();
        }
    };
    const glm::mat4 lightProjection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 20.0f);
    glm::vec3 spotPosition(0, 2, 3), spotDirection(0, -.5f, -1);
    glm::vec3 spotColor(0);
    int pointCount = 0, spotCount = 0;
    std::array<glm::vec3, 8> pointPositions{}, pointColors{};
    std::array<glm::vec3, 4> spotPositions{}, spotDirections{}, spotColors{};
    float environmentIntensity = 0;
    bool skyVisible = false;
    glm::mat3 environmentRotation(1);
    bool environmentSelected = false;
    for (const auto &object : scene.objects()) {
        if (!object.enabledInHierarchy())
            continue;
        if (object.kind == SceneObjectKind::PointLight && pointCount < 8) {
            const auto &settings = dynamic_cast<const LightObject &>(object).settings;
            pointPositions[pointCount] = object.worldPosition();
            pointColors[pointCount++] = settings.color * settings.intensity;
        }
        if (object.kind == SceneObjectKind::SpotLight && spotCount < 4) {
            const auto &settings = dynamic_cast<const LightObject &>(object).settings;
            spotPositions[spotCount] = object.worldPosition();
            spotDirections[spotCount] =
                glm::normalize(glm::vec3(object.editorMatrix() * glm::vec4(0, 0, -1, 0)));
            spotColors[spotCount++] = settings.color * settings.intensity;
        }
        if (object.kind == SceneObjectKind::Environment) {
            const auto &settings = dynamic_cast<const EnvironmentObject &>(object).settings;
            environmentIntensity += settings.intensity;
            skyVisible |= settings.sky;
            if (!environmentSelected) {
                glm::mat3 rotation(object.editorMatrix());
                for (int axis = 0; axis < 3; ++axis)
                    rotation[axis] = glm::normalize(rotation[axis]);
                environmentRotation = glm::transpose(rotation);
                environmentSelected = true;
            }
        }
    }
    if (spotCount) {
        spotPosition = spotPositions[0];
        spotDirection = spotDirections[0];
        spotColor = spotColors[0];
    }
    const auto up = std::abs(spotDirection.y) > .99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::mat4 lightSpaceMatrix =
        lightProjection * glm::lookAt(spotPosition, spotPosition + spotDirection, up);

    spotlightShadowMap_.beginWrite();
    depthProgram_.use();
    glUniformMatrix4fv(depthLightSpaceLocation_, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glUniform1i(depthUseInstancingLocation_, GL_TRUE);
    drawObjShadows();
    glUniform1i(depthUseInstancingLocation_, GL_FALSE);
    for (const auto &platform : scene.objects()) {
        if (showOnlyImportedModel_ || !platform.enabledInHierarchy() ||
            platform.kind != SceneObjectKind::Platform)
            continue;
        const auto floorTransform = platform.matrix();
        glUniformMatrix4fv(depthModelLocation_, 1, GL_FALSE, glm::value_ptr(floorTransform));
        floorMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
    }
    spotlightShadowMap_.endWrite();

    const glm::mat4 pointProjection =
        glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, pointShadowFarPlane);
    const std::array<glm::vec3, 6> directions = {{{1.0f, 0.0f, 0.0f},
                                                  {-1.0f, 0.0f, 0.0f},
                                                  {0.0f, 1.0f, 0.0f},
                                                  {0.0f, -1.0f, 0.0f},
                                                  {0.0f, 0.0f, 1.0f},
                                                  {0.0f, 0.0f, -1.0f}}};
    const std::array<glm::vec3, 6> upDirections = {{{0.0f, -1.0f, 0.0f},
                                                    {0.0f, -1.0f, 0.0f},
                                                    {0.0f, 0.0f, 1.0f},
                                                    {0.0f, 0.0f, -1.0f},
                                                    {0.0f, -1.0f, 0.0f},
                                                    {0.0f, -1.0f, 0.0f}}};

    depthProgram_.use();
    for (int face = 0; face < 6; ++face) {
        pointShadowMap_.beginFace(face);

        const glm::mat4 faceView =
            glm::lookAt(lightPosition, lightPosition + directions[face], upDirections[face]);
        const glm::mat4 faceLightSpace = pointProjection * faceView;
        glUniformMatrix4fv(depthLightSpaceLocation_, 1, GL_FALSE, glm::value_ptr(faceLightSpace));
        glUniform1i(depthUseInstancingLocation_, GL_TRUE);
        drawObjShadows();
        glUniform1i(depthUseInstancingLocation_, GL_FALSE);
        for (const auto &platform : scene.objects()) {
            if (showOnlyImportedModel_ || !platform.enabledInHierarchy() ||
                platform.kind != SceneObjectKind::Platform)
                continue;
            const auto floorTransform = platform.matrix();
            glUniformMatrix4fv(depthModelLocation_, 1, GL_FALSE, glm::value_ptr(floorTransform));
            floorMesh.draw();
            ++stats_.drawCalls;
            stats_.submittedTriangles += 2;
        }
    }

    pointShadowMap_.endWrite();

    if (!sceneTarget_.resize(framebufferWidth, framebufferHeight) ||
        !blurBuffer_.resize(framebufferWidth, framebufferHeight)) {
        return;
    }
    sceneTarget_.begin();
    glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mainProgram_.use();
    glUniform1i(mainProgram_.uniform("uPointCount"), pointCount);
    if (pointCount) {
        glUniform3fv(mainProgram_.uniform("uPointPositions"), pointCount,
                     glm::value_ptr(pointPositions[0]));
        glUniform3fv(mainProgram_.uniform("uPointColors"), pointCount,
                     glm::value_ptr(pointColors[0]));
    }
    glUniform1i(mainProgram_.uniform("uSpotCount"), spotCount);
    if (spotCount) {
        glUniform3fv(mainProgram_.uniform("uSpotPositions"), spotCount,
                     glm::value_ptr(spotPositions[0]));
        glUniform3fv(mainProgram_.uniform("uSpotDirections"), spotCount,
                     glm::value_ptr(spotDirections[0]));
        glUniform3fv(mainProgram_.uniform("uSpotColors"), spotCount, glm::value_ptr(spotColors[0]));
    }
    glUniform1f(mainProgram_.uniform("uEnvironmentIntensity"), environmentIntensity);
    glUniformMatrix3fv(mainProgram_.uniform("uEnvironmentRotation"), 1, GL_FALSE,
                       glm::value_ptr(environmentRotation));
    glUniform1i(mainProgram_.uniform("uOverrideRoughness"), GL_FALSE);
    glUniform3fv(mainProgram_.uniform("uSpotLightColor"), 1, glm::value_ptr(spotColor));
    glUniformMatrix4fv(transformLocation_, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniformMatrix4fv(lightSpaceMatrixLocation_, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glUniform3fv(cameraPositionLocation_, 1, glm::value_ptr(camera.position()));
    glUniform3fv(spotLightPositionLocation_, 1, glm::value_ptr(spotPosition));
    glUniform3fv(spotLightDirectionLocation_, 1, glm::value_ptr(spotDirection));
    glUniform3fv(lightPositionLocation_, 1, glm::value_ptr(lightPosition));
    glUniform3fv(lightPosition2Location_, 1, glm::value_ptr(lightPosition2));

    fallbackTexture.bind(0);
    spotlightShadowMap_.bind(1);
    pointShadowMap_.bind(2);
    normalTexture.bind(3);
    environmentIbl_.bind(4, 5, 6);

    glUniform1i(usePbrMapsLocation_, GL_FALSE);
    glUniform1f(textureScaleLocation_, 1.0f);
    glUniform1f(metallicLocation_, modelMetallic);
    glUniform1f(roughnessLocation_, modelRoughness);
    glUniform1f(aoLocation_, 1.0f);
    glUniform1i(useInstancingLocation_, GL_TRUE);
    for (auto &batch : batches) {
        if (batch.visible.empty())
            continue;
        batch.mesh->updateInstanceTransforms(batch.visible.data(), batch.visible.size());
        glUniform1f(metallicLocation_, batch.metallic);
        glUniform1f(roughnessLocation_, batch.roughness);
        for (const auto &part : batch.model->parts()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, batch.materials->diffuseTextureId(part.diffuseTextureName,
                                                                           fallbackTexture.id()));
            glUniform3fv(materialColorLocation_, 1, glm::value_ptr(part.diffuseColor));
            batch.mesh->drawRangeInstanced(part.firstIndex, part.indexCount);
            ++stats_.drawCalls;
            stats_.submittedTriangles += part.indexCount / 3 * batch.visible.size();
        }
    }
    glUniform1i(useInstancingLocation_, GL_FALSE);

    for (const auto &platform : scene.objects()) {
        if (showOnlyImportedModel_ || !platform.enabledInHierarchy() ||
            platform.kind != SceneObjectKind::Platform)
            continue;
        const auto floorTransform = platform.matrix();
        const auto &settings = dynamic_cast<const PlatformObject &>(platform);
        glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(floorTransform));
        glUniform3fv(materialColorLocation_, 1, glm::value_ptr(settings.tint));
        glUniform1f(metallicLocation_, 0.0f);
        glUniform1f(roughnessLocation_, settings.roughness);
        glUniform1i(mainProgram_.uniform("uOverrideRoughness"), GL_TRUE);
        glUniform1f(aoLocation_, 1.0f);
        glUniform1i(usePbrMapsLocation_, floorMaterial.valid() ? GL_TRUE : GL_FALSE);
        glUniform1f(textureScaleLocation_, 1.0f);
        if (floorMaterial.valid()) {
            floorMaterial.bind(0, 3, 7, 8);
        } else {
            fallbackTexture.bind(0);
            normalTexture.bind(3);
        }
        floorMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
    }

    // Animation evaluation belongs to SimulationRuntime, never this draw pass.
    for (const auto &object : scene.objects()) {
        if (object.kind != SceneObjectKind::Skinned || !object.enabledInHierarchy() ||
            !showAnimatedModel_)
            continue;
        auto *animation = simulations_.animation(object.id);
        const auto transform = object.matrix();
        if (!animation || !animation->valid() ||
            !frustum.containsSphere(transformedCenter(transform, animation->boundsCenter()),
                                    animation->boundsRadius() * maximumScale(transform)))
            continue;
        animation->draw(viewProjection, transform, camera.position(), lightPosition, pointColors[0],
                        environmentIntensity);
        ++stats_.drawCalls;
        stats_.submittedTriangles += animation->triangleCount();
    }
    struct GltfDraw {
        GltfScene *asset;
        glm::mat4 transform;
    };
    std::vector<GltfDraw> visibleGltf;
    for (const auto &object : scene.objects()) {
        if (object.kind != SceneObjectKind::Gltf || !object.enabledInHierarchy() || !showGltfScene_)
            continue;
        auto *asset = assets_.gltf(dynamic_cast<const ModelObject &>(object).model.asset);
        const auto transform = object.matrix();
        if (!asset || !asset->valid() ||
            !frustum.containsSphere(transformedCenter(transform, asset->center()),
                                    asset->radius() * maximumScale(transform)))
            continue;
        visibleGltf.push_back({asset, transform});
        asset->draw(viewProjection, transform, camera.position(), lightPosition, false,
                    pointColors[0], environmentIntensity);
        stats_.drawCalls += asset->drawCount(false);
        stats_.submittedTriangles += asset->triangleCount(false);
    }

    if (!showOnlyImportedModel_ && skyVisible) {
        environmentIbl_.renderSkybox(view, projection, environmentRotation, environmentIntensity);
        ++stats_.drawCalls;
        stats_.submittedTriangles += 12;
    }

    // Transparent instances are submitted far-to-near; each asset also sorts
    // its own transparent primitives. Interpenetrating surfaces remain limited.
    std::stable_sort(visibleGltf.begin(), visibleGltf.end(), [&](const auto &a, const auto &b) {
        const auto da = transformedCenter(a.transform, a.asset->center()) - camera.position();
        const auto db = transformedCenter(b.transform, b.asset->center()) - camera.position();
        return glm::dot(da, da) > glm::dot(db, db);
    });
    for (const auto &submission : visibleGltf) {
        auto &asset = *submission.asset;
        asset.draw(viewProjection, submission.transform, camera.position(), lightPosition, true,
                   pointColors[0], environmentIntensity);
        stats_.drawCalls += asset.drawCount(true);
        stats_.submittedTriangles += asset.triangleCount(true);
    }

    for (const auto &object : scene.objects()) {
        if (!object.enabledInHierarchy() || object.kind != SceneObjectKind::Water ||
            !object.waterSettings().visible)
            continue;
        auto &runtime = fluidSystem(object.id);
        sceneTarget_.copyColorForSampling();
        sceneTarget_.copyDepthForSampling();
        sceneTarget_.bindColorCopy(0);
        sceneTarget_.bindDepthCopy(1);
        environmentIbl_.bind(2, 4, 5);
        runtime.draw(viewProjection, camera.position(), lightPosition, framebufferWidth,
                     framebufferHeight, object.waterSettings(), object.matrix(), pointColors[0],
                     environmentRotation, environmentIntensity);
        ++stats_.drawCalls;
        if (object.waterSettings().viewMode != 1)
            stats_.submittedTriangles += runtime.triangleCount();
    }
    for (const auto &object : scene.objects())
        if (object.enabledInHierarchy() && object.kind == SceneObjectKind::Smoke) {
            smoke2D(object.id).drawScene(viewProjection, object.editorMatrix(),
                                         object.smokeSettings().opacity);
            ++stats_.drawCalls;
            stats_.submittedTriangles += 2;
        }

    if (stats_.liveParticles > 0) {
        sceneTarget_.copyDepthForSampling();
        sceneTarget_.bindDepthCopy(0);
        std::vector<const SceneObject *> emitters;
        for (const auto &object : scene.objects())
            if (object.enabledInHierarchy() && object.kind == SceneObjectKind::CpuEmitter)
                emitters.push_back(&object);
        std::stable_sort(emitters.begin(), emitters.end(), [&](const auto *a, const auto *b) {
            const auto da = a->worldPosition() - camera.position(),
                       db = b->worldPosition() - camera.position();
            return glm::dot(da, da) > glm::dot(db, db);
        });
        for (const auto *object : emitters) {
            const auto runtime = simulations_.cpuEmitters_.find(object->id);
            if (runtime == simulations_.cpuEmitters_.end() || !runtime->second->valid() ||
                !runtime->second->count())
                continue;
            runtime->second->draw(viewProjection, view, camera.position(),
                                  object->particleSettings().preset,
                                  object->particleSettings().soft, object->editorMatrix());
            ++stats_.drawCalls;
            stats_.submittedTriangles += 2 * runtime->second->count();
        }
    }

    for (const auto &object : scene.objects()) {
        if (!object.enabledInHierarchy() || object.kind != SceneObjectKind::GpuEmitter ||
            !object.gpuSettings().visible)
            continue;
        const auto runtime = simulations_.gpuEmitters_.find(object.id);
        if (runtime == simulations_.gpuEmitters_.end() || !runtime->second->valid())
            continue;
        runtime->second->draw(viewProjection, view, object.gpuSettings(), object.editorMatrix());
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2 * runtime->second->slotCount(object.gpuSettings());
    }

    // Light markers are intentionally hidden in the imported-model test scene.

    if (showShadowMap) {
        glDisable(GL_DEPTH_TEST);
        debugProgram_.use();
        spotlightShadowMap_.bind(0);
        debugMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
        glEnable(GL_DEPTH_TEST);
    }

    sceneTarget_.end();
    glDisable(GL_DEPTH_TEST);

    // Extract HDR pixels brighter than the threshold.
    blurBuffer_.beginWrite(0);
    brightPassProgram_.use();
    sceneTarget_.bindColorTexture(0);
    debugMesh.draw();
    ++stats_.drawCalls;
    stats_.submittedTriangles += 2;

    // A separable Gaussian blur alternates horizontal and vertical passes.
    constexpr int blurPassCount = 10;
    blurProgram_.use();
    for (int pass = 0; pass < blurPassCount; ++pass) {
        const int sourceIndex = pass % 2;
        const int targetIndex = (pass + 1) % 2;
        blurBuffer_.beginWrite(targetIndex);
        glUniform1i(blurHorizontalLocation_, pass % 2 == 0 ? GL_TRUE : GL_FALSE);
        blurBuffer_.bindTexture(sourceIndex, 0);
        debugMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
    }

    sceneTarget_.end();
    if (!viewportTarget_.resize(framebufferWidth, framebufferHeight))
        return;
    viewportTarget_.begin();
    postprocessProgram_.use();
    glUniform1i(grayscaleLocation_, useGrayscale ? GL_TRUE : GL_FALSE);
    glUniform1i(bloomEnabledLocation_, bloomEnabled ? GL_TRUE : GL_FALSE);
    glUniform1f(exposureLocation_, exposure);
    sceneTarget_.bindColorTexture(0);
    blurBuffer_.bindTexture(blurPassCount % 2, 1);
    debugMesh.draw();
    ++stats_.drawCalls;
    stats_.submittedTriangles += 2;
    viewportTarget_.end();
    glEnable(GL_DEPTH_TEST);
}

void Renderer::resetSimulation() { simulations_.reset(); }
void Renderer::destroy() {
    valid_ = false;
    spotlightShadowMap_.destroy();
    pointShadowMap_.destroy();
    sceneTarget_.destroy();
    viewportTarget_.destroy();
    blurBuffer_.destroy();
    environmentIbl_.destroy();
    mainProgram_.destroy();
    lightProgram_.destroy();
    depthProgram_.destroy();
    debugProgram_.destroy();
    brightPassProgram_.destroy();
    blurProgram_.destroy();
    postprocessProgram_.destroy();
}

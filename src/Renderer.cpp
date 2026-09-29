#include "Renderer.h"

#include "Camera.h"
#include "Frustum.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "PbrMaterial.h"
#include "Scene.h"
#include "Texture2D.h"

#include <array>
#include <algorithm>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr GLsizei shadowMapSize = 1024;
constexpr GLsizei pointShadowMapSize = 1024;
constexpr float pointShadowFarPlane = 25.0f;

float maximumScale(const glm::mat4& transform) {
    return std::max({
        glm::length(glm::vec3(transform[0])),
        glm::length(glm::vec3(transform[1])),
        glm::length(glm::vec3(transform[2]))
    });
}

glm::vec3 transformedCenter(
    const glm::mat4& transform,
    const glm::vec3& localCenter
) {
    return glm::vec3(transform * glm::vec4(localCenter, 1.0f));
}

} // namespace

Renderer::Renderer(
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& environmentPath,
    const std::filesystem::path& animatedModelPath
)
    : mainProgram_(
          shaderDirectory / "main.vert",
          shaderDirectory / "main.frag",
          "Main shader"
      ),
      lightProgram_(
          shaderDirectory / "light_marker.vert",
          shaderDirectory / "light_marker.frag",
          "Light marker shader"
      ),
      depthProgram_(
          shaderDirectory / "depth.vert",
          shaderDirectory / "depth.frag",
          "Depth shader"
      ),
      debugProgram_(
          shaderDirectory / "debug.vert",
          shaderDirectory / "debug.frag",
          "Debug shader"
      ),
      brightPassProgram_(
          shaderDirectory / "postprocess.vert",
          shaderDirectory / "bright_pass.frag",
          "Bright-pass shader"
      ),
      blurProgram_(
          shaderDirectory / "postprocess.vert",
          shaderDirectory / "blur.frag",
          "Gaussian blur shader"
      ),
      postprocessProgram_(
          shaderDirectory / "postprocess.vert",
          shaderDirectory / "postprocess.frag",
          "Post-process shader"
      ),
      environmentIbl_(shaderDirectory, environmentPath),
      animatedModel_(shaderDirectory, animatedModelPath),
      gltfScene_(shaderDirectory),
      gpuParticleSystem_(shaderDirectory),
      smoke2D_(shaderDirectory),
      fluidSystem_(shaderDirectory),
      particleSystem_(shaderDirectory) {
    if (!mainProgram_.valid()
        || !lightProgram_.valid()
        || !depthProgram_.valid()
        || !debugProgram_.valid()
        || !brightPassProgram_.valid()
        || !blurProgram_.valid()
        || !postprocessProgram_.valid()
        || !environmentIbl_.valid()
        || !particleSystem_.valid()
        || !fluidSystem_.valid()
        || !gpuParticleSystem_.valid()
        || !smoke2D_.valid()) {
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
    spotLightPositionLocation_ =
        mainProgram_.uniform("uSpotLightPosition");
    spotLightDirectionLocation_ =
        mainProgram_.uniform("uSpotLightDirection");
    cameraPositionLocation_ = mainProgram_.uniform("uCameraPosition");
    lightSpaceMatrixLocation_ =
        mainProgram_.uniform("uLightSpaceMatrix");
    lightTransformLocation_ = lightProgram_.uniform("uViewProjection");
    markerLightPositionLocation_ = lightProgram_.uniform("uLightPosition");
    markerColorLocation_ = lightProgram_.uniform("uMarkerColor");
    depthLightSpaceLocation_ = depthProgram_.uniform("uLightSpaceMatrix");
    depthModelLocation_ = depthProgram_.uniform("uModel");
    depthUseInstancingLocation_ =
        depthProgram_.uniform("uUseInstancing");
    grayscaleLocation_ = postprocessProgram_.uniform("uGrayscale");
    bloomEnabledLocation_ =
        postprocessProgram_.uniform("uBloomEnabled");
    exposureLocation_ = postprocessProgram_.uniform("uExposure");
    blurHorizontalLocation_ = blurProgram_.uniform("uHorizontal");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    if (!spotlightShadowMap_.create(shadowMapSize)
        || !pointShadowMap_.create(pointShadowMapSize)) {
        return;
    }

    configureStaticUniforms();
    valid_ = true;
}

Renderer::~Renderer() {
    destroy();
}

bool Renderer::valid() const {
    return valid_;
}

const RendererStats& Renderer::stats() const {
    return stats_;
}

GLuint Renderer::viewportTexture() const {
    return viewportTarget_.colorTexture();
}

void Renderer::setShowOnlyImportedModel(bool enabled) {
    showOnlyImportedModel_ = enabled;
}

ParticleSettings& Renderer::particleSettings() { return particleSettings_; }
bool& Renderer::showGltfModel() { return showAnimatedModel_; }
GltfAnimatedModel& Renderer::gltfModel() { return animatedModel_; }
GltfScene& Renderer::gltfScene() { return gltfScene_; }
bool& Renderer::showGltfScene() { return showGltfScene_; }
FluidSettings& Renderer::fluidSettings() { return fluidSettings_; }
FluidSystem& Renderer::fluidSystem() { return fluidSystem_; }
GpuParticleSettings& Renderer::gpuParticleSettings() { return gpuParticleSettings_; }
GpuParticleSystem& Renderer::gpuParticleSystem() { return gpuParticleSystem_; }
Fluid2DSettings& Renderer::smokeSettings() { return smokeSettings_; }
Fluid2D& Renderer::smoke2D() { return smoke2D_; }

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
    glUniform1f(
        mainProgram_.uniform("uPointShadowFarPlane"),
        pointShadowFarPlane
    );
    glUniform3f(mainProgram_.uniform("uLightColor"), 3.0f, 2.7f, 2.2f);
    glUniform3f(
        mainProgram_.uniform("uLightColor2"),
        0.5f,
        1.0f,
        3.0f
    );
    glUniform3f(
        mainProgram_.uniform("uSpotLightColor"),
        2.0f,
        2.0f,
        2.0f
    );
    glUniform1f(
        mainProgram_.uniform("uSpotInnerCutoff"),
        glm::cos(glm::radians(12.5f))
    );
    glUniform1f(
        mainProgram_.uniform("uSpotOuterCutoff"),
        glm::cos(glm::radians(17.5f))
    );
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

void Renderer::render(
    const Scene& scene,
    const Camera& camera,
    int framebufferWidth,
    int framebufferHeight,
    Mesh& modelMesh,
    const Mesh& floorMesh,
    const Mesh& debugMesh,
    const Model& model,
    const Texture2D& fallbackTexture,
    const Texture2D& normalTexture,
    const MaterialLibrary& materialLibrary,
    const PbrMaterial& floorMaterial,
    bool showShadowMap,
    bool useGrayscale,
    bool bloomEnabled,
    float exposure,
    float modelMetallic,
    float modelRoughness,
    float timeSeconds
) {
    stats_ = {};
    const auto& modelTransforms = scene.modelTransforms();
    const glm::mat4& floorTransform = scene.floorTransform();
    const glm::vec3& lightPosition = scene.primaryLightPosition();
    const glm::vec3& lightPosition2 = scene.secondaryLightPosition();

    const glm::mat4 view = camera.viewMatrix();
    const glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(framebufferWidth)
            / static_cast<float>(framebufferHeight),
        0.1f,
        100.0f
    );
    const glm::mat4 viewProjection = projection * view;
    const Frustum frustum(viewProjection);

    smoke2D_.update(timeSeconds - lastSmokeTime_, smokeSettings_);
    lastSmokeTime_ = timeSeconds;
    stats_.smokeUpdateMs = smoke2D_.updateMilliseconds();
    stats_.smokePasses = smoke2D_.passCount();
    stats_.drawCalls += static_cast<std::uint64_t>(stats_.smokePasses);
    stats_.submittedTriangles += 2 * static_cast<std::uint64_t>(stats_.smokePasses);
    gpuParticleSystem_.update(timeSeconds - lastGpuParticleTime_, gpuParticleSettings_);
    lastGpuParticleTime_ = timeSeconds;
    stats_.gpuParticleSlots = gpuParticleSystem_.slotCount(gpuParticleSettings_);

    // Keep all instances for shadow passes: an off-screen object may still
    // cast a shadow into the camera view.
    stats_.totalInstances = modelTransforms.size();
    modelMesh.updateInstanceTransforms(
        modelTransforms.data(),
        modelTransforms.size()
    );

    // Cull complete instances for the camera color pass.
    std::vector<glm::mat4> visibleModelTransforms;
    visibleModelTransforms.reserve(modelTransforms.size());
    for (const glm::mat4& transform : modelTransforms) {
        const glm::vec3 center = transformedCenter(
            transform,
            model.boundsCenter()
        );
        const float radius = model.boundsRadius() * maximumScale(transform);
        if (frustum.containsSphere(center, radius)) {
            visibleModelTransforms.push_back(transform);
        }
    }
    stats_.visibleInstances = visibleModelTransforms.size();

    const glm::mat4 animatedTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(-2.2f, -0.5f, -1.2f))
        * glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
    const glm::mat4 gltfTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(2.1f, 0.4f, -1.0f))
        * glm::scale(glm::mat4(1.0f), glm::vec3(
            0.75f / std::max(gltfScene_.radius(), 0.001f)));
    animatedModel_.update(timeSeconds);
    fluidSystem_.update(timeSeconds - lastFluidTime_, fluidSettings_);
    lastFluidTime_ = timeSeconds;
    stats_.fluidParticles = fluidSystem_.particleCount();
    stats_.fluidTriangles = fluidSystem_.triangleCount();
    stats_.fluidUpdateMs = fluidSystem_.updateMilliseconds();
    particleSystem_.update(timeSeconds - lastParticleTime_, particleSettings_);
    lastParticleTime_ = timeSeconds;
    stats_.liveParticles = particleSystem_.count();
    const bool animatedModelVisible = animatedModel_.valid()
        && frustum.containsSphere(
            transformedCenter(
                animatedTransform,
                animatedModel_.boundsCenter()
            ),
            animatedModel_.boundsRadius() * maximumScale(animatedTransform)
        );
    const glm::mat4 lightProjection = glm::perspective(
        glm::radians(45.0f),
        1.0f,
        0.1f,
        20.0f
    );
    const glm::mat4 lightSpaceMatrix = lightProjection * camera.viewMatrix();

    spotlightShadowMap_.beginWrite();
    depthProgram_.use();
    glUniformMatrix4fv(
        depthLightSpaceLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(lightSpaceMatrix)
    );
    glUniform1i(depthUseInstancingLocation_, GL_TRUE);
    if (!modelTransforms.empty()) {
        modelMesh.drawInstanced();
        ++stats_.drawCalls;
        stats_.submittedTriangles += (model.indices().size() / 3)
            * modelTransforms.size();
    }
    glUniform1i(depthUseInstancingLocation_, GL_FALSE);
    if (!showOnlyImportedModel_) {
        glUniformMatrix4fv(
            depthModelLocation_,
            1,
            GL_FALSE,
            glm::value_ptr(floorTransform)
        );
        floorMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
    }
    spotlightShadowMap_.endWrite();

    const glm::mat4 pointProjection = glm::perspective(
        glm::radians(90.0f),
        1.0f,
        0.1f,
        pointShadowFarPlane
    );
    const std::array<glm::vec3, 6> directions = {{
        { 1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},
        { 0.0f,  0.0f,  1.0f},
        { 0.0f,  0.0f, -1.0f}
    }};
    const std::array<glm::vec3, 6> upDirections = {{
        {0.0f, -1.0f,  0.0f},
        {0.0f, -1.0f,  0.0f},
        {0.0f,  0.0f,  1.0f},
        {0.0f,  0.0f, -1.0f},
        {0.0f, -1.0f,  0.0f},
        {0.0f, -1.0f,  0.0f}
    }};

    depthProgram_.use();
    for (int face = 0; face < 6; ++face) {
        pointShadowMap_.beginFace(face);

        const glm::mat4 faceView = glm::lookAt(
            lightPosition,
            lightPosition + directions[face],
            upDirections[face]
        );
        const glm::mat4 faceLightSpace = pointProjection * faceView;
        glUniformMatrix4fv(
            depthLightSpaceLocation_,
            1,
            GL_FALSE,
            glm::value_ptr(faceLightSpace)
        );
        glUniform1i(depthUseInstancingLocation_, GL_TRUE);
        if (!modelTransforms.empty()) {
            modelMesh.drawInstanced();
            ++stats_.drawCalls;
            stats_.submittedTriangles += (model.indices().size() / 3)
                * modelTransforms.size();
        }
        glUniform1i(depthUseInstancingLocation_, GL_FALSE);
        if (!showOnlyImportedModel_) {
            glUniformMatrix4fv(
                depthModelLocation_,
                1,
                GL_FALSE,
                glm::value_ptr(floorTransform)
            );
            floorMesh.draw();
            ++stats_.drawCalls;
            stats_.submittedTriangles += 2;
        }
    }

    pointShadowMap_.endWrite();

    if (!sceneTarget_.resize(framebufferWidth, framebufferHeight)
        || !blurBuffer_.resize(framebufferWidth, framebufferHeight)) {
        return;
    }
    modelMesh.updateInstanceTransforms(
        visibleModelTransforms.data(),
        visibleModelTransforms.size()
    );
    sceneTarget_.begin();
    glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mainProgram_.use();
    glUniformMatrix4fv(
        transformLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(viewProjection)
    );
    glUniformMatrix4fv(
        lightSpaceMatrixLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(lightSpaceMatrix)
    );
    glUniform3fv(
        cameraPositionLocation_,
        1,
        glm::value_ptr(camera.position())
    );
    glUniform3fv(
        spotLightPositionLocation_,
        1,
        glm::value_ptr(camera.position())
    );
    glUniform3fv(
        spotLightDirectionLocation_,
        1,
        glm::value_ptr(camera.front())
    );
    glUniform3fv(
        lightPositionLocation_,
        1,
        glm::value_ptr(lightPosition)
    );
    glUniform3fv(
        lightPosition2Location_,
        1,
        glm::value_ptr(lightPosition2)
    );

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
    if (!visibleModelTransforms.empty()) {
        for (const ModelPart& part : model.parts()) {
            const GLuint partTexture = materialLibrary.diffuseTextureId(
                part.diffuseTextureName,
                fallbackTexture.id()
            );
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, partTexture);
            glUniform3fv(
                materialColorLocation_,
                1,
                glm::value_ptr(part.diffuseColor)
            );
            modelMesh.drawRangeInstanced(part.firstIndex, part.indexCount);
            ++stats_.drawCalls;
            stats_.submittedTriangles += (part.indexCount / 3)
                * visibleModelTransforms.size();
        }
    }
    glUniform1i(useInstancingLocation_, GL_FALSE);

    if (!showOnlyImportedModel_) {
        glUniformMatrix4fv(
            modelLocation_,
            1,
            GL_FALSE,
            glm::value_ptr(floorTransform)
        );
        glUniform3f(materialColorLocation_, 1.0f, 1.0f, 1.0f);
        glUniform1f(metallicLocation_, 0.0f);
        glUniform1f(roughnessLocation_, 0.78f);
        glUniform1f(aoLocation_, 1.0f);
        glUniform1i(
            usePbrMapsLocation_,
            floorMaterial.valid() ? GL_TRUE : GL_FALSE
        );
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

    // The glTF joints are evaluated on the CPU; the vertex skinning itself
    // happens in skinned.vert on the GPU.
    if (showAnimatedModel_ && animatedModelVisible) {
        animatedModel_.draw(
            viewProjection,
            animatedTransform,
            camera.position(),
            lightPosition
        );
        ++stats_.drawCalls;
        stats_.submittedTriangles += animatedModel_.triangleCount();
    }

    const bool gltfVisible = showGltfScene_ && gltfScene_.valid()
        && frustum.containsSphere(
            transformedCenter(gltfTransform, gltfScene_.center()),
            gltfScene_.radius() * maximumScale(gltfTransform));
    if (gltfVisible) {
        gltfScene_.draw(viewProjection, gltfTransform, camera.position(),
                        lightPosition, false);
        stats_.drawCalls += gltfScene_.drawCount(false);
        stats_.submittedTriangles += gltfScene_.triangleCount(false);
    }

    if (!showOnlyImportedModel_) {
        environmentIbl_.renderSkybox(view, projection);
        ++stats_.drawCalls;
        stats_.submittedTriangles += 12;
    }

    if (gltfVisible) {
        gltfScene_.draw(viewProjection, gltfTransform, camera.position(),
                        lightPosition, true);
        stats_.drawCalls += gltfScene_.drawCount(true);
        stats_.submittedTriangles += gltfScene_.triangleCount(true);
    }

    if (fluidSettings_.visible) {
        sceneTarget_.copyColorForSampling();
        sceneTarget_.copyDepthForSampling();
        sceneTarget_.bindColorCopy(0);
        sceneTarget_.bindDepthCopy(1);
        environmentIbl_.bind(2, 4, 5);
        fluidSystem_.draw(viewProjection, camera.position(), lightPosition,
                          framebufferWidth, framebufferHeight, fluidSettings_);
        ++stats_.drawCalls;
        if (fluidSettings_.viewMode != 1)
            stats_.submittedTriangles += fluidSystem_.triangleCount();
    }

    if (particleSystem_.count() > 0) {
        sceneTarget_.copyDepthForSampling();
        sceneTarget_.bindDepthCopy(0);
        particleSystem_.draw(viewProjection, view, camera.position(),
                             particleSettings_.preset, particleSettings_.soft);
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2 * particleSystem_.count();
    }

    if (gpuParticleSettings_.visible) {
        gpuParticleSystem_.draw(viewProjection, view, gpuParticleSettings_);
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2 * stats_.gpuParticleSlots;
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
        glUniform1i(
            blurHorizontalLocation_,
            pass % 2 == 0 ? GL_TRUE : GL_FALSE
        );
        blurBuffer_.bindTexture(sourceIndex, 0);
        debugMesh.draw();
        ++stats_.drawCalls;
        stats_.submittedTriangles += 2;
    }

    sceneTarget_.end();
    if (!viewportTarget_.resize(framebufferWidth, framebufferHeight)) return;
    viewportTarget_.begin();
    postprocessProgram_.use();
    glUniform1i(grayscaleLocation_, useGrayscale ? GL_TRUE : GL_FALSE);
    glUniform1i(
        bloomEnabledLocation_,
        bloomEnabled ? GL_TRUE : GL_FALSE
    );
    glUniform1f(exposureLocation_, exposure);
    sceneTarget_.bindColorTexture(0);
    blurBuffer_.bindTexture(blurPassCount % 2, 1);
    debugMesh.draw();
    ++stats_.drawCalls;
    stats_.submittedTriangles += 2;
    viewportTarget_.end();
    glEnable(GL_DEPTH_TEST);
}

void Renderer::destroy() {
    valid_ = false;
    spotlightShadowMap_.destroy();
    pointShadowMap_.destroy();
    sceneTarget_.destroy();
    viewportTarget_.destroy();
    blurBuffer_.destroy();
    animatedModel_.destroy();
    gltfScene_.destroy();
    gpuParticleSystem_.destroy();
    smoke2D_.destroy();
    fluidSystem_.destroy();
    particleSystem_.destroy();
    environmentIbl_.destroy();
    mainProgram_.destroy();
    lightProgram_.destroy();
    depthProgram_.destroy();
    debugProgram_.destroy();
    brightPassProgram_.destroy();
    blurProgram_.destroy();
    postprocessProgram_.destroy();
}

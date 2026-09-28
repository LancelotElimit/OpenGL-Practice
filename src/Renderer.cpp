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
      animatedModel_(shaderDirectory, animatedModelPath) {
    if (!mainProgram_.valid()
        || !lightProgram_.valid()
        || !depthProgram_.valid()
        || !debugProgram_.valid()
        || !brightPassProgram_.valid()
        || !blurProgram_.valid()
        || !postprocessProgram_.valid()
        || !environmentIbl_.valid()) {
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

    // Cull complete instances on the CPU before updating the instance VBO.
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
    modelMesh.updateInstanceTransforms(
        visibleModelTransforms.data(),
        visibleModelTransforms.size()
    );

    const glm::mat4 animatedTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(-2.2f, -0.5f, -1.2f))
        * glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
    animatedModel_.update(timeSeconds);
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
    modelMesh.drawInstanced();
    glUniform1i(depthUseInstancingLocation_, GL_FALSE);
    glUniformMatrix4fv(
        depthModelLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(floorTransform)
    );
    floorMesh.draw();
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
        modelMesh.drawInstanced();
        glUniform1i(depthUseInstancingLocation_, GL_FALSE);
        glUniformMatrix4fv(
            depthModelLocation_,
            1,
            GL_FALSE,
            glm::value_ptr(floorTransform)
        );
        floorMesh.draw();
    }

    pointShadowMap_.endWrite();

    if (!sceneTarget_.resize(framebufferWidth, framebufferHeight)
        || !blurBuffer_.resize(framebufferWidth, framebufferHeight)) {
        return;
    }
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
    }
    glUniform1i(useInstancingLocation_, GL_FALSE);

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

    // The glTF joints are evaluated on the CPU; the vertex skinning itself
    // happens in skinned.vert on the GPU.
    if (animatedModelVisible) {
        animatedModel_.draw(
            viewProjection,
            animatedTransform,
            camera.position(),
            lightPosition
        );
    }

    environmentIbl_.renderSkybox(view, projection);

    glDisable(GL_DEPTH_TEST);
    lightProgram_.use();
    glUniformMatrix4fv(
        lightTransformLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(viewProjection)
    );
    glUniform3fv(
        markerLightPositionLocation_,
        1,
        glm::value_ptr(lightPosition)
    );
    glUniform3f(markerColorLocation_, 4.0f, 3.4f, 0.8f);
    glDrawArrays(GL_POINTS, 0, 1);
    glUniform3fv(
        markerLightPositionLocation_,
        1,
        glm::value_ptr(lightPosition2)
    );
    glUniform3f(markerColorLocation_, 0.5f, 1.0f, 4.0f);
    glDrawArrays(GL_POINTS, 0, 1);
    glEnable(GL_DEPTH_TEST);

    if (showShadowMap) {
        glDisable(GL_DEPTH_TEST);
        debugProgram_.use();
        spotlightShadowMap_.bind(0);
        debugMesh.draw();
        glEnable(GL_DEPTH_TEST);
    }

    sceneTarget_.end();
    glDisable(GL_DEPTH_TEST);

    // Extract HDR pixels brighter than the threshold.
    blurBuffer_.beginWrite(0);
    brightPassProgram_.use();
    sceneTarget_.bindColorTexture(0);
    debugMesh.draw();

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
    }

    sceneTarget_.end();
    glViewport(0, 0, framebufferWidth, framebufferHeight);
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
    glEnable(GL_DEPTH_TEST);
}

void Renderer::destroy() {
    valid_ = false;
    spotlightShadowMap_.destroy();
    pointShadowMap_.destroy();
    sceneTarget_.destroy();
    blurBuffer_.destroy();
    animatedModel_.destroy();
    environmentIbl_.destroy();
    mainProgram_.destroy();
    lightProgram_.destroy();
    depthProgram_.destroy();
    debugProgram_.destroy();
    brightPassProgram_.destroy();
    blurProgram_.destroy();
    postprocessProgram_.destroy();
}

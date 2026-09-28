#pragma once

#include "BlurBuffer.h"
#include "EnvironmentIBL.h"
#include "GltfAnimatedModel.h"
#include "RenderTarget.h"
#include "ShaderProgram.h"
#include "ShadowMap.h"

#include <filesystem>

#include <glad/gl.h>

class Camera;
class MaterialLibrary;
class Mesh;
class Model;
class PbrMaterial;
class Scene;
class Texture2D;

class Renderer {
public:
    Renderer(
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& environmentPath,
        const std::filesystem::path& animatedModelPath
    );
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool valid() const;
    void setShowOnlyImportedModel(bool enabled);
    void render(
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
    );
    void destroy();

private:
    void configureStaticUniforms();

    ShaderProgram mainProgram_;
    ShaderProgram lightProgram_;
    ShaderProgram depthProgram_;
    ShaderProgram debugProgram_;
    ShaderProgram brightPassProgram_;
    ShaderProgram blurProgram_;
    ShaderProgram postprocessProgram_;
    EnvironmentIBL environmentIbl_;
    GltfAnimatedModel animatedModel_;

    ShadowMap2D spotlightShadowMap_;
    ShadowCubeMap pointShadowMap_;
    RenderTarget sceneTarget_;
    BlurBuffer blurBuffer_;
    bool showOnlyImportedModel_ = false;
    bool showAnimatedModel_ = false;
    bool valid_ = false;

    GLint transformLocation_ = -1;
    GLint modelLocation_ = -1;
    GLint materialColorLocation_ = -1;
    GLint metallicLocation_ = -1;
    GLint roughnessLocation_ = -1;
    GLint aoLocation_ = -1;
    GLint usePbrMapsLocation_ = -1;
    GLint textureScaleLocation_ = -1;
    GLint lightPositionLocation_ = -1;
    GLint lightPosition2Location_ = -1;
    GLint spotLightPositionLocation_ = -1;
    GLint spotLightDirectionLocation_ = -1;
    GLint cameraPositionLocation_ = -1;
    GLint lightSpaceMatrixLocation_ = -1;
    GLint lightTransformLocation_ = -1;
    GLint markerLightPositionLocation_ = -1;
    GLint markerColorLocation_ = -1;
    GLint depthLightSpaceLocation_ = -1;
    GLint depthModelLocation_ = -1;
    GLint useInstancingLocation_ = -1;
    GLint depthUseInstancingLocation_ = -1;
    GLint grayscaleLocation_ = -1;
    GLint bloomEnabledLocation_ = -1;
    GLint exposureLocation_ = -1;
    GLint blurHorizontalLocation_ = -1;
};

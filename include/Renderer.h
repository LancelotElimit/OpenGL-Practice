#pragma once

#include "BlurBuffer.h"
#include "EnvironmentIBL.h"
#include "Fluid2D.h"
#include "FluidSystem.h"
#include "GltfAnimatedModel.h"
#include "GltfScene.h"
#include "GpuParticleSystem.h"
#include "ParticleSystem.h"
#include "RenderTarget.h"
#include "ShaderProgram.h"
#include "ShadowMap.h"

#include <filesystem>
#include <cstddef>
#include <cstdint>

#include <glad/gl.h>

class Camera;
class MaterialLibrary;
class Mesh;
class Model;
class PbrMaterial;
class Scene;
class Texture2D;

struct RendererStats {
    std::uint64_t drawCalls = 0;
    std::uint64_t submittedTriangles = 0;
    std::size_t visibleInstances = 0;
    std::size_t totalInstances = 0;
    std::size_t liveParticles = 0;
    std::size_t fluidParticles = 0;
    std::size_t fluidTriangles = 0;
    float fluidUpdateMs = 0.0f;
    std::size_t gpuParticleSlots = 0;
    float smokeUpdateMs = 0.0f;
    int smokePasses = 0;
};

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
    const RendererStats& stats() const;
    GLuint viewportTexture() const;
    void setShowOnlyImportedModel(bool enabled);
    ParticleSettings& particleSettings();
    bool& showGltfModel();
    GltfAnimatedModel& gltfModel();
    GltfScene& gltfScene();
    bool& showGltfScene();
    FluidSettings& fluidSettings();
    FluidSystem& fluidSystem();
    GpuParticleSettings& gpuParticleSettings();
    GpuParticleSystem& gpuParticleSystem();
    Fluid2DSettings& smokeSettings();
    Fluid2D& smoke2D();
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
    GltfScene gltfScene_;
    GpuParticleSystem gpuParticleSystem_;
    GpuParticleSettings gpuParticleSettings_;
    float lastGpuParticleTime_ = 0.0f;
    Fluid2D smoke2D_;
    Fluid2DSettings smokeSettings_;
    float lastSmokeTime_ = 0.0f;
    FluidSystem fluidSystem_;
    FluidSettings fluidSettings_;
    float lastFluidTime_ = 0.0f;
    ParticleSystem particleSystem_;
    ParticleSettings particleSettings_;
    float lastParticleTime_ = 0.0f;

    ShadowMap2D spotlightShadowMap_;
    ShadowCubeMap pointShadowMap_;
    RenderTarget sceneTarget_;
    RenderTarget viewportTarget_;
    BlurBuffer blurBuffer_;
    bool showOnlyImportedModel_ = false;
    bool showAnimatedModel_ = false;
    bool showGltfScene_ = false;
    bool valid_ = false;
    RendererStats stats_{};

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

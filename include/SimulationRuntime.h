#pragma once
#include "Fluid2D.h"
#include "FluidSystem.h"
#include "GltfAnimatedModel.h"
#include "GpuParticleSystem.h"
#include "ParticleSystem.h"
#include <memory>
#include <unordered_map>
class Scene;
class AssetLibrary;
struct SimulationStats {
    std::size_t liveParticles = 0, fluidParticles = 0, fluidTriangles = 0, gpuParticleSlots = 0;
    float fluidUpdateMs = 0, smokeUpdateMs = 0;
    int smokePasses = 0;
};
// Live simulation is independent of rendering and authored Scene snapshots.
class SimulationRuntime {
  public:
    SimulationRuntime(const std::filesystem::path &shaders);
    void update(const Scene &scene, float delta, float time, bool advance, AssetLibrary &assets);
    void reset();
    void resetEmitter(std::uint32_t id);
    FluidSystem &fluidSystem(std::uint32_t id);
    Fluid2D &smoke2D(std::uint32_t id);
    GltfAnimatedModel *animation(std::uint32_t id);
    const SimulationStats &stats() const { return stats_; }

  private:
    friend class Renderer;
    std::filesystem::path particleShaderDirectory_;
    struct AnimationState {
        std::string source;
        std::unique_ptr<GltfAnimatedModel> model;
    };
    std::unordered_map<std::uint32_t, AnimationState> animations_;
    std::unordered_map<std::uint32_t, std::unique_ptr<ParticleSystem>> cpuEmitters_;
    std::unordered_map<std::uint32_t, std::unique_ptr<GpuParticleSystem>> gpuEmitters_;
    std::unordered_map<std::uint32_t, std::unique_ptr<Fluid2D>> smokeObjects_;
    std::unordered_map<std::uint32_t, std::unique_ptr<FluidSystem>> waterObjects_;
    SimulationStats stats_;
};

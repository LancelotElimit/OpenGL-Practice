#include "SimulationRuntime.h"
#include "AssetLibrary.h"
#include "FrameClock.h"
#include "Scene.h"
#include <algorithm>
SimulationRuntime::SimulationRuntime(const std::filesystem::path &shaders)
    : particleShaderDirectory_(shaders) {}
FluidSystem &SimulationRuntime::fluidSystem(std::uint32_t id) {
    auto &runtime = waterObjects_[id];
    if (!runtime)
        runtime = std::make_unique<FluidSystem>(particleShaderDirectory_);
    return *runtime;
}
Fluid2D &SimulationRuntime::smoke2D(std::uint32_t id) {
    auto &runtime = smokeObjects_[id];
    if (!runtime)
        runtime = std::make_unique<Fluid2D>(particleShaderDirectory_);
    return *runtime;
}
void SimulationRuntime::resetEmitter(std::uint32_t id) {
    cpuEmitters_.erase(id);
    gpuEmitters_.erase(id);
}
void SimulationRuntime::reset() {
    cpuEmitters_.clear();
    gpuEmitters_.clear();
    waterObjects_.clear();
    smokeObjects_.clear();
    animations_.clear();
    stats_ = {};
}
GltfAnimatedModel *SimulationRuntime::animation(std::uint32_t id) {
    auto it = animations_.find(id);
    return it == animations_.end() ? nullptr : it->second.model.get();
}
void SimulationRuntime::update(const Scene &scene, float delta, float timeSeconds,
                               bool updateSimulation, AssetLibrary &assets) {
    stats_ = {};
    const float dt = FrameClock::simulationDelta(delta);
    // Runtime state is keyed by stable scene ID. Copies never share live buffers.
    const auto hasEmitter = [&scene](std::uint32_t id, SceneObjectKind kind) {
        return std::any_of(scene.objects().begin(), scene.objects().end(), [&](const auto &object) {
            return object.id == id && object.kind == kind;
        });
    };
    std::erase_if(cpuEmitters_, [&](const auto &item) {
        return !hasEmitter(item.first, SceneObjectKind::CpuEmitter);
    });
    std::erase_if(gpuEmitters_, [&](const auto &item) {
        return !hasEmitter(item.first, SceneObjectKind::GpuEmitter);
    });
    std::erase_if(waterObjects_, [&](const auto &item) {
        return !hasEmitter(item.first, SceneObjectKind::Water);
    });
    std::erase_if(smokeObjects_, [&](const auto &item) {
        return !hasEmitter(item.first, SceneObjectKind::Smoke);
    });
    for (const auto &object : scene.objects()) {
        if (!object.enabledInHierarchy())
            continue;
        if (object.kind == SceneObjectKind::Water) {
            auto &runtime = fluidSystem(object.id);
            if (updateSimulation)
                runtime.update(dt, object.waterSettings());
            stats_.fluidParticles += runtime.particleCount();
            stats_.fluidTriangles += runtime.triangleCount();
            if (updateSimulation)
                stats_.fluidUpdateMs += runtime.updateMilliseconds();
        } else if (object.kind == SceneObjectKind::Smoke) {
            auto &runtime = smoke2D(object.id);
            if (updateSimulation)
                runtime.update(dt, object.smokeSettings());
            if (updateSimulation) {
                stats_.smokeUpdateMs += runtime.updateMilliseconds();
                stats_.smokePasses += runtime.passCount();
            }
        } else if (object.kind == SceneObjectKind::CpuEmitter) {
            auto &runtime = cpuEmitters_[object.id];
            if (!runtime)
                runtime = std::make_unique<ParticleSystem>(particleShaderDirectory_);
            if (runtime->valid()) {
                if (updateSimulation)
                    runtime->update(dt, object.particleSettings());
                stats_.liveParticles += runtime->count();
            }
        } else if (object.kind == SceneObjectKind::GpuEmitter && object.gpuSettings().visible) {
            auto &runtime = gpuEmitters_[object.id];
            if (!runtime) {
                runtime = std::make_unique<GpuParticleSystem>(particleShaderDirectory_);
                runtime->reset(object.gpuSettings());
            }
            if (runtime->valid()) {
                if (updateSimulation)
                    runtime->update(dt, object.gpuSettings());
                stats_.gpuParticleSlots += runtime->slotCount(object.gpuSettings());
            }
        }
    }

    std::erase_if(animations_, [&](const auto &item) {
        return !hasEmitter(item.first, SceneObjectKind::Skinned);
    });
    for (const auto &object : scene.objects()) {
        if (object.kind != SceneObjectKind::Skinned)
            continue;
        const auto &component = dynamic_cast<const ModelObject &>(object).model;
        auto &entry = animations_[object.id];
        const auto source = component.asset.source.empty()
                                ? assets.defaultAnimated().generic_string()
                                : assets.resolve(component.asset).generic_string();
        if (entry.source != source || !entry.model) {
            entry.source = source;
            entry.model = std::make_unique<GltfAnimatedModel>(particleShaderDirectory_, source);
        }
        auto &animation = *entry.model;
        animation.playing() = component.playing;
        animation.playbackSpeed() = component.playbackSpeed;
        if (component.clip < static_cast<int>(animation.animationCount()))
            animation.setAnimationIndex(component.clip);
        if (updateSimulation && object.enabledInHierarchy())
            animation.update(timeSeconds);
    }
}

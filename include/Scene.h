#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <array>
#include <memory>

#include <glm/glm.hpp>
#include "ParticleSettings.h"
#include "SimulationSettings.h"
#include <iterator>

enum class SceneObjectKind { Obj, Gltf, Skinned, CpuEmitter, GpuEmitter, Water, Smoke, Platform, PointLight, Camera, Environment, SpotLight, Count };
struct LightSettings { glm::vec3 color{1}; float intensity = 3; };
struct EnvironmentSettings { bool sky = true; float intensity = 1; };
struct SceneObject {
    virtual ~SceneObject() = default;
    virtual std::unique_ptr<SceneObject> clone() const = 0;
    std::uint32_t id = 0;
    std::string name;
    SceneObjectKind kind = SceneObjectKind::Obj;
    bool visible = true;
    glm::vec3 position{0}, rotation{0}, scale{1};
    glm::mat4 importTransform{1};
    glm::vec3 boundsCenter{0};
    float boundsRadius = 1;
    std::shared_ptr<const std::vector<glm::vec3>> pickingTriangles;
    ParticleSettings& particleSettings();
    GpuParticleSettings& gpuSettings();
    FluidSettings& waterSettings();
    Fluid2DSettings& smokeSettings();
    const ParticleSettings& particleSettings() const;
    const GpuParticleSettings& gpuSettings() const;
    const FluidSettings& waterSettings() const;
    const Fluid2DSettings& smokeSettings() const;
    glm::mat4 editorMatrix() const;
    glm::mat4 matrix() const;
    float intersectRay(const glm::vec3& origin, const glm::vec3& direction) const;
};
// Use shallow inheritance for genuine behavior categories, and composition for
// editable settings. Cloning preserves the concrete class without slicing.
struct RenderableObject : SceneObject {};
struct SimulationObject : RenderableObject {};
struct ModelObject final : RenderableObject {
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<ModelObject>(*this); }
};
struct PlatformObject final : RenderableObject {
    glm::vec3 tint{1}; float roughness = .78f;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<PlatformObject>(*this); }
};
struct ParticleEmitterObject final : SimulationObject {
    ParticleSettings particles; GpuParticleSettings gpuParticles;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<ParticleEmitterObject>(*this); }
};
struct WaterObject final : SimulationObject {
    FluidSettings settings;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<WaterObject>(*this); }
};
struct SmokeObject final : SimulationObject {
    Fluid2DSettings settings;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<SmokeObject>(*this); }
};
struct LightObject final : SceneObject {
    LightSettings settings;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<LightObject>(*this); }
};
struct CameraObject final : SceneObject {
    float fov = 45;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<CameraObject>(*this); }
};
struct EnvironmentObject final : SceneObject {
    EnvironmentSettings settings;
    std::unique_ptr<SceneObject> clone() const override { return std::make_unique<EnvironmentObject>(*this); }
};

class SceneObjects {
    std::vector<std::unique_ptr<SceneObject>> storage_;
public:
    SceneObjects() = default;
    SceneObjects(const SceneObjects& other) { for (const auto& object : other.storage_) storage_.push_back(object->clone()); }
    SceneObjects& operator=(const SceneObjects& other) { SceneObjects copy(other); storage_.swap(copy.storage_); return *this; }
    SceneObjects(SceneObjects&&) = default;
    SceneObjects& operator=(SceneObjects&&) = default;
    template<class It, class Ref> struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = SceneObject;
        using difference_type = std::ptrdiff_t;
        using reference = Ref;
        using pointer = std::remove_reference_t<Ref>*;
        It it;
        Ref operator*() const { return **it; }
        pointer operator->() const { return it->get(); }
        Iterator& operator++() { ++it; return *this; }
        Iterator operator++(int) { auto copy = *this; ++it; return copy; }
        bool operator==(const Iterator& other) const { return it == other.it; }
    };
    auto begin() { return Iterator<decltype(storage_.begin()), SceneObject&>{storage_.begin()}; }
    auto end() { return Iterator<decltype(storage_.end()), SceneObject&>{storage_.end()}; }
    auto begin() const { return Iterator<decltype(storage_.begin()), const SceneObject&>{storage_.begin()}; }
    auto end() const { return Iterator<decltype(storage_.end()), const SceneObject&>{storage_.end()}; }
    void push_back(std::unique_ptr<SceneObject> object) { storage_.push_back(std::move(object)); }
    void clear() { storage_.clear(); }
    std::size_t size() const { return storage_.size(); }
    bool empty() const { return storage_.empty(); }
    SceneObject& operator[](std::size_t index) { return *storage_[index]; }
    bool remove(std::uint32_t id);
};
struct ImportedGeometry {
    glm::mat4 importTransform{1}; glm::vec3 boundsCenter{0}; float boundsRadius = 1;
    std::shared_ptr<const std::vector<glm::vec3>> pickingTriangles;
};
class Scene {
public:
    Scene();
    void configureObject(SceneObjectKind kind, const glm::mat4& import,
                         const glm::vec3& center, float radius);
    SceneObject* find(std::uint32_t id);
    const SceneObjects& objects() const { return objects_; }
    void clear();
    static const char* typeName(SceneObjectKind kind);
    std::uint32_t duplicate(std::uint32_t id);
    bool remove(std::uint32_t id);
    std::uint32_t add(SceneObjectKind kind);
    void setPickingTriangles(SceneObjectKind kind, std::vector<glm::vec3> triangles);
    void setModelImportTransform(const glm::mat4& transform);
    void update(float timeSeconds);

    const std::vector<glm::mat4>& modelTransforms() const;
    const glm::mat4& floorTransform() const;
    const glm::vec3& primaryLightPosition() const;
    const glm::vec3& secondaryLightPosition() const;

private:
    SceneObjects objects_;
    std::array<ImportedGeometry, static_cast<int>(SceneObjectKind::Count)> assetTemplates_;
    std::uint32_t nextId_ = 1;
    std::vector<glm::mat4> modelTransforms_;
    glm::mat4 floorTransform_{1.0f};
    glm::vec3 primaryLightPosition_{2.0f, 1.5f, 0.0f};
    glm::vec3 secondaryLightPosition_{-2.0f, 0.75f, 0.0f};
};

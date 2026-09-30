#include "Scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

ParticleSettings& SceneObject::particleSettings() { return dynamic_cast<ParticleEmitterObject&>(*this).particles; }
GpuParticleSettings& SceneObject::gpuSettings() { return dynamic_cast<ParticleEmitterObject&>(*this).gpuParticles; }
FluidSettings& SceneObject::waterSettings() { return dynamic_cast<WaterObject&>(*this).settings; }
Fluid2DSettings& SceneObject::smokeSettings() { return dynamic_cast<SmokeObject&>(*this).settings; }
const ParticleSettings& SceneObject::particleSettings() const { return dynamic_cast<const ParticleEmitterObject&>(*this).particles; }
const GpuParticleSettings& SceneObject::gpuSettings() const { return dynamic_cast<const ParticleEmitterObject&>(*this).gpuParticles; }
const FluidSettings& SceneObject::waterSettings() const { return dynamic_cast<const WaterObject&>(*this).settings; }
const Fluid2DSettings& SceneObject::smokeSettings() const { return dynamic_cast<const SmokeObject&>(*this).settings; }
bool SceneObjects::remove(std::uint32_t id) {
    return std::erase_if(storage_, [id](const auto& object) { return object->id == id; }) != 0;
}
const char* Scene::typeName(SceneObjectKind kind) {
    static const char* names[] = {"OBJ", "glTF", "Skinned Model", "CPU Particle Emitter", "GPU Particle Emitter",
        "Water", "Smoke", "Platform", "Point Light", "Camera", "Environment", "Spot Light"};
    return names[static_cast<int>(kind)];
}

glm::mat4 SceneObject::editorMatrix() const {
    return glm::translate(glm::mat4(1), position)
        * glm::rotate(glm::mat4(1), glm::radians(rotation.z), glm::vec3(0,0,1))
        * glm::rotate(glm::mat4(1), glm::radians(rotation.y), glm::vec3(0,1,0))
        * glm::rotate(glm::mat4(1), glm::radians(rotation.x), glm::vec3(1,0,0))
        * glm::scale(glm::mat4(1), scale);
}
glm::mat4 SceneObject::matrix() const { return editorMatrix() * importTransform; }
float SceneObject::intersectRay(const glm::vec3& origin, const glm::vec3& direction) const {
    const float miss = std::numeric_limits<float>::infinity();
    const auto inverse = glm::inverse(matrix());
    const auto localOrigin = glm::vec3(inverse * glm::vec4(origin,1));
    // Do not normalize: the parameter must remain a world-space distance.
    const auto ray = glm::vec3(inverse * glm::vec4(direction,0));
    const auto relative = localOrigin - boundsCenter;
    const float a = glm::dot(ray,ray), b = glm::dot(relative,ray);
    const float c = glm::dot(relative,relative) - boundsRadius*boundsRadius;
    const float discriminant = b*b-a*c;
    if (a < 1e-12f || discriminant < 0) return miss;
    float distance = (-b-std::sqrt(discriminant))/a;
    if (distance < 0) distance = (-b+std::sqrt(discriminant))/a;
    if (distance < 0) return miss;
    if (!pickingTriangles || pickingTriangles->empty()) return distance;
    float closest = miss;
    for (std::size_t i = 0; i+2 < pickingTriangles->size(); i+=3) {
        const auto v0 = (*pickingTriangles)[i];
        const auto edge1 = (*pickingTriangles)[i+1]-v0;
        const auto edge2 = (*pickingTriangles)[i+2]-v0;
        const auto cross = glm::cross(ray,edge2);
        const float determinant = glm::dot(edge1,cross);
        if (std::abs(determinant) < 1e-8f) continue;
        const float invDet = 1/determinant;
        const auto offset = localOrigin-v0;
        const float u = glm::dot(offset,cross)*invDet;
        if (u < 0 || u > 1) continue;
        const auto q = glm::cross(offset,edge1);
        const float v = glm::dot(ray,q)*invDet;
        if (v < 0 || u+v > 1) continue;
        const float t = glm::dot(edge2,q)*invDet;
        if (t >= 0) closest = std::min(closest,t);
    }
    return closest;
}
Scene::Scene() = default;
void Scene::clear() { objects_.clear(); modelTransforms_.clear(); nextId_ = 1; }
std::uint32_t Scene::add(SceneObjectKind kind) {
    std::unique_ptr<SceneObject> instance;
    switch (kind) {
    case SceneObjectKind::CpuEmitter: case SceneObjectKind::GpuEmitter: instance = std::make_unique<ParticleEmitterObject>(); break;
    case SceneObjectKind::Water: instance = std::make_unique<WaterObject>(); break;
    case SceneObjectKind::Smoke: instance = std::make_unique<SmokeObject>(); break;
    case SceneObjectKind::Platform: instance = std::make_unique<PlatformObject>(); break;
    case SceneObjectKind::PointLight: case SceneObjectKind::SpotLight: instance = std::make_unique<LightObject>(); break;
    case SceneObjectKind::Camera: instance = std::make_unique<CameraObject>(); break;
    case SceneObjectKind::Environment: instance = std::make_unique<EnvironmentObject>(); break;
    default: instance = std::make_unique<ModelObject>(); break;
    }
    auto& object = *instance;
    const auto& asset = assetTemplates_[static_cast<int>(kind)];
    object.importTransform = asset.importTransform; object.boundsCenter = asset.boundsCenter;
    object.boundsRadius = asset.boundsRadius; object.pickingTriangles = asset.pickingTriangles;
    object.id = nextId_++;
    object.kind = kind;
    object.name = typeName(kind);
    if (kind == SceneObjectKind::CpuEmitter || kind == SceneObjectKind::GpuEmitter)
        object.boundsRadius = .16f; // editor handle, not the dynamic particle cloud
    if (kind == SceneObjectKind::PointLight || kind == SceneObjectKind::SpotLight || kind == SceneObjectKind::Camera)
        object.boundsRadius = .15f;
    if (kind == SceneObjectKind::Platform) {
        object.position.y = -.5f;
        object.boundsRadius = 7.1f;
        object.pickingTriangles = std::make_shared<const std::vector<glm::vec3>>(
            std::initializer_list<glm::vec3>{{-5,0,-5},{5,0,-5},{5,0,5},{5,0,5},{-5,0,5},{-5,0,-5}});
    }
    if (kind == SceneObjectKind::Smoke) {
        object.scale = {1.5f,1.5f,1}; object.boundsRadius=1.42f;
        object.pickingTriangles=std::make_shared<const std::vector<glm::vec3>>(
            std::initializer_list<glm::vec3>{{-1,-1,0},{1,-1,0},{1,1,0},{1,1,0},{-1,1,0},{-1,-1,0}});
    }
    if (kind == SceneObjectKind::Water) {
        object.importTransform = glm::translate(glm::mat4(1),glm::vec3(-1.15f,-.25f,0));
        object.boundsCenter = {1.15f,.25f,0};
    }
    const auto id = object.id;
    objects_.push_back(std::move(instance));
    return id;
}
SceneObject* Scene::find(std::uint32_t id) {
    for (auto& object : objects_) if (object.id == id) return &object;
    return nullptr;
}
std::uint32_t Scene::duplicate(std::uint32_t id) {
    const auto* source = find(id);
    if (!source) return 0;
    auto copy = source->clone();
    copy->id = nextId_++;
    copy->name += " Copy";
    copy->position.x += .35f;
    const auto idCopy = copy->id;
    objects_.push_back(std::move(copy));
    return idCopy;
}
bool Scene::remove(std::uint32_t id) {
    return objects_.remove(id);
}
void Scene::configureObject(SceneObjectKind kind, const glm::mat4& import,
                            const glm::vec3& center, float radius) {
    auto& asset = assetTemplates_[static_cast<int>(kind)];
    asset.importTransform = import;
    asset.boundsCenter = center;
    asset.boundsRadius = radius;
    for (auto& object : objects_) if (object.kind == kind) {
        object.importTransform = import;
        object.boundsCenter = center;
        object.boundsRadius = radius;
    }
}
void Scene::setPickingTriangles(SceneObjectKind kind, std::vector<glm::vec3> triangles) {
    auto geometry = std::make_shared<const std::vector<glm::vec3>>(std::move(triangles));
    assetTemplates_[static_cast<int>(kind)].pickingTriangles = geometry;
    for (auto& object : objects_) if (object.kind == kind) object.pickingTriangles = geometry;
}

void Scene::setModelImportTransform(const glm::mat4& transform) {
    configureObject(SceneObjectKind::Obj, transform, glm::vec3(0), 1);
}


void Scene::update(float) {
    // Update render transforms from editable instances, not from animation time.
    int lightIndex = 0;
    for (const auto& object : objects_) if (object.visible && object.kind == SceneObjectKind::PointLight) {
        if (lightIndex++ == 0) primaryLightPosition_ = object.position;
        else { secondaryLightPosition_ = object.position; break; }
    }

    modelTransforms_.clear();
    for (const auto& object : objects_)
        if (object.kind == SceneObjectKind::Obj && object.visible)
            modelTransforms_.push_back(object.matrix());
}

const std::vector<glm::mat4>& Scene::modelTransforms() const {
    return modelTransforms_;
}

const glm::mat4& Scene::floorTransform() const {
    return floorTransform_;
}

const glm::vec3& Scene::primaryLightPosition() const {
    return primaryLightPosition_;
}

const glm::vec3& Scene::secondaryLightPosition() const {
    return secondaryLightPosition_;
}

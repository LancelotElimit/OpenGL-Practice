#include "AssetLibrary.h"
#include "GltfAnimatedModel.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
AssetLibrary::AssetLibrary(std::filesystem::path shaders, std::filesystem::path root,
                           std::filesystem::path animated, TextureCache &textures)
    : shaders_(std::move(shaders)), root_(std::move(root)), defaultAnimated_(std::move(animated)),
      textures_(textures), legacyGltf_(shaders_) {}
std::filesystem::path AssetLibrary::resolve(const AssetReference &ref) const {
    auto path = std::filesystem::path(ref.source);
    if (!path.is_absolute())
        path = root_ / path;
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    return error ? std::filesystem::absolute(path).lexically_normal() : canonical;
}
AssetReference AssetLibrary::reference(const std::filesystem::path &path) const {
    auto absolute = std::filesystem::absolute(path).lexically_normal();
    auto relative = absolute.lexically_relative(root_);
    return {relative.empty() ? absolute.generic_string() : relative.generic_string()};
}
AssetLibrary::ObjAsset *AssetLibrary::obj(const AssetReference &ref) {
    if (ref.source.empty())
        return nullptr;
    const auto path = resolve(ref);
    const auto key = path.generic_string();
    if (auto it = objs_.find(key); it != objs_.end())
        return it->second.get();
    if (failures_.contains(key)) {
        status_ = failures_.at(key);
        return nullptr;
    }
    auto asset = std::make_unique<ObjAsset>();
    if (!asset->model.load(path) || asset->model.indices().empty()) {
        status_ = "OBJ import failed: " + key;
        failures_[key] = status_;
        return nullptr;
    }
    const auto &model = asset->model;
    asset->mesh.uploadIndexed(model.vertices().data(), model.vertices().size(),
                              Model::VertexStrideFloats,
                              {{0, 3, 0}, {1, 2, 3}, {2, 3, 5}, {3, 4, 8}}, model.indices().data(),
                              model.indices().size());
    asset->materials.loadForModel(model, path.parent_path(), textures_);
    asset->geometry.boundsCenter = model.boundsCenter();
    asset->geometry.boundsRadius = model.boundsRadius();
    asset->geometry.importTransform =
        glm::scale(glm::mat4(1), glm::vec3(.75f / std::max(model.boundsRadius(), .001f))) *
        glm::translate(glm::mat4(1), -model.boundsCenter());
    std::vector<glm::vec3> triangles;
    for (auto index : model.indices()) {
        const auto offset = index * Model::VertexStrideFloats;
        triangles.emplace_back(model.vertices()[offset], model.vertices()[offset + 1],
                               model.vertices()[offset + 2]);
    }
    asset->geometry.pickingTriangles =
        std::make_shared<const std::vector<glm::vec3>>(std::move(triangles));
    auto *result = asset.get();
    objs_.emplace(key, std::move(asset));
    status_ = "Loaded OBJ: " + key;
    return result;
}
GltfScene *AssetLibrary::gltf(const AssetReference &ref) {
    if (ref.source.empty())
        return &legacyGltf_;
    const auto path = resolve(ref);
    const auto key = path.generic_string();
    if (auto it = gltfs_.find(key); it != gltfs_.end())
        return it->second.get();
    if (failures_.contains(key)) {
        status_ = failures_.at(key);
        return nullptr;
    }
    auto asset = std::make_unique<GltfScene>(shaders_);
    if (!asset->load(path)) {
        status_ = asset->status();
        failures_[key] = status_;
        return nullptr;
    }
    picking_[key] = std::make_shared<const std::vector<glm::vec3>>(asset->pickingTriangles());
    auto *result = asset.get();
    gltfs_.emplace(key, std::move(asset));
    status_ = "Loaded glTF: " + key;
    return result;
}
std::uint32_t AssetLibrary::importModel(Scene &scene, SceneObjectKind kind,
                                        const std::filesystem::path &path) {
    const auto ref = reference(path);
    failures_.erase(resolve(ref).generic_string());
    if (kind == SceneObjectKind::Obj && !obj(ref))
        return 0;
    if (kind == SceneObjectKind::Gltf && !gltf(ref))
        return 0;
    if (kind == SceneObjectKind::Skinned) {
        GltfAnimatedModel validation(shaders_, resolve(ref));
        if (!validation.valid()) {
            status_ = "No supported skin in: " + path.string();
            return 0;
        }
    }
    if (kind != SceneObjectKind::Obj && kind != SceneObjectKind::Gltf &&
        kind != SceneObjectKind::Skinned)
        return 0;
    const auto id = scene.add(kind);
    auto &object = dynamic_cast<ModelObject &>(*scene.find(id));
    object.name = path.stem().string();
    object.model.asset = ref;
    prepare(scene);
    status_ = "Added independent model: " + path.filename().string();
    return id;
}
void AssetLibrary::prepare(Scene &scene) {
    for (const auto &item : scene.objects()) {
        auto *object = dynamic_cast<ModelObject *>(scene.find(item.id));
        if (!object || object->model.asset.source.empty())
            continue;
        ImportedGeometry geometry;
        if (object->kind == SceneObjectKind::Obj) {
            auto *asset = obj(object->model.asset);
            if (!asset)
                continue;
            geometry = asset->geometry;
        } else if (object->kind == SceneObjectKind::Gltf) {
            auto *asset = gltf(object->model.asset);
            if (!asset)
                continue;
            geometry.boundsCenter = asset->center();
            geometry.boundsRadius = asset->radius();
            geometry.importTransform =
                glm::scale(glm::mat4(1), glm::vec3(.75f / std::max(asset->radius(), .001f))) *
                glm::translate(glm::mat4(1), -asset->center());
            // Cache pointer on object; avoid copying triangles on every frame.
            geometry.pickingTriangles = picking_.at(resolve(object->model.asset).generic_string());
        } else
            continue;
        object->importTransform = geometry.importTransform;
        object->boundsCenter = geometry.boundsCenter;
        object->boundsRadius = geometry.boundsRadius;
        object->pickingTriangles = geometry.pickingTriangles;
    }
}
bool AssetLibrary::bindModel(Scene &scene, std::uint32_t id, const std::filesystem::path &path) {
    auto *object = dynamic_cast<ModelObject *>(scene.find(id));
    if (!object) {
        status_ = "Select a model object first.";
        return false;
    }
    const auto ref = reference(path);
    failures_.erase(resolve(ref).generic_string());
    const auto extension = path.extension().string();
    if (object->kind == SceneObjectKind::Obj && extension != ".obj") {
        status_ = "OBJ objects require an OBJ asset.";
        return false;
    }
    if (object->kind != SceneObjectKind::Obj && extension != ".gltf" && extension != ".glb") {
        status_ = "glTF objects require glTF/GLB.";
        return false;
    }
    if (object->kind == SceneObjectKind::Obj && !obj(ref))
        return false;
    if (object->kind == SceneObjectKind::Gltf && !gltf(ref))
        return false;
    if (object->kind == SceneObjectKind::Skinned) {
        GltfAnimatedModel validation(shaders_, resolve(ref));
        if (!validation.valid()) {
            status_ = "No supported skin in asset.";
            return false;
        }
    }
    object->model.asset = ref;
    object->model.clip = 0;
    prepare(scene);
    status_ = "Bound model asset; other instances unchanged.";
    return true;
}

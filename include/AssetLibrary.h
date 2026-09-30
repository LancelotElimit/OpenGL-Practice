#pragma once
#include "GltfScene.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Scene.h"
#include "TextureCache.h"
#include <filesystem>
#include <unordered_map>
// Import and resource lifetime boundary. Objects store references, not GL names.
class AssetLibrary {
  public:
    struct ObjAsset {
        Model model;
        Mesh mesh;
        MaterialLibrary materials;
        ImportedGeometry geometry;
    };
    AssetLibrary(std::filesystem::path shaders, std::filesystem::path root,
                 std::filesystem::path animated, TextureCache &textures);
    std::filesystem::path resolve(const AssetReference &reference) const;
    AssetReference reference(const std::filesystem::path &path) const;
    ObjAsset *obj(const AssetReference &reference);
    GltfScene *gltf(const AssetReference &reference);
    GltfScene &legacyGltf() { return legacyGltf_; }
    const std::filesystem::path &defaultAnimated() const { return defaultAnimated_; }
    bool bindModel(Scene &scene, std::uint32_t id, const std::filesystem::path &path);
    std::uint32_t importModel(Scene &scene, SceneObjectKind kind,
                              const std::filesystem::path &path);
    void prepare(Scene &scene);
    std::size_t resourceCount() const { return objs_.size() + gltfs_.size(); }
    std::size_t textureCount() const { return textures_.size(); }
    const std::string &status() const { return status_; }

  private:
    std::filesystem::path shaders_, root_, defaultAnimated_;
    TextureCache &textures_; // engine session owns the shared texture cache
    GltfScene legacyGltf_;
    std::unordered_map<std::string, std::unique_ptr<ObjAsset>> objs_;
    std::unordered_map<std::string, std::unique_ptr<GltfScene>> gltfs_;
    std::unordered_map<std::string, std::shared_ptr<const std::vector<glm::vec3>>> picking_;
    std::unordered_map<std::string, std::string> failures_;
    std::string status_;
};

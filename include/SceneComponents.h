#pragma once
#include <glm/glm.hpp>
#include <string>
// Authored data components contain no GPU resources.
struct TransformComponent {
    glm::vec3 position{0}, rotation{0}, scale{1};
};
struct AssetReference {
    std::string source; // asset-root relative, or absolute for external assets
    bool operator==(const AssetReference &) const = default;
};
struct ModelComponent {
    AssetReference asset;
    bool playing = true;
    int clip = 0;
    float playbackSpeed = 1;
    bool overrideMaterial = false;
    float metallic = .3f, roughness = .3f;
};

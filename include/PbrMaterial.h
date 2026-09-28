#pragma once

#include <filesystem>
#include <glad/gl.h>

class Texture2D;
class TextureCache;

// A complete texture set for one metallic/roughness PBR material.
class PbrMaterial {
public:
    bool load(
        const std::filesystem::path& albedoPath,
        const std::filesystem::path& normalPath,
        const std::filesystem::path& roughnessPath,
        const std::filesystem::path& aoPath,
        TextureCache& textureCache
    );
    bool valid() const;
    void bind(
        GLuint albedoUnit,
        GLuint normalUnit,
        GLuint roughnessUnit,
        GLuint aoUnit
    ) const;
    void destroy();

private:
    const Texture2D* albedo_ = nullptr;
    const Texture2D* normal_ = nullptr;
    const Texture2D* roughness_ = nullptr;
    const Texture2D* ao_ = nullptr;
    bool valid_ = false;
};

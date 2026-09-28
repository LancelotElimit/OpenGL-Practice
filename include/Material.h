#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <glad/gl.h>

class Model;
class Texture2D;
class TextureCache;

// Maps model material names to textures owned by the shared cache.
class MaterialLibrary {
public:
    void loadForModel(
        const Model& model,
        const std::filesystem::path& modelDirectory,
        TextureCache& textureCache
    );
    GLuint diffuseTextureId(
        const std::string& textureName,
        GLuint fallbackTexture
    ) const;
    void destroy();

private:
    std::unordered_map<std::string, const Texture2D*> diffuseTextures_;
};

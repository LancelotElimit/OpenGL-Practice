#include "Material.h"

#include "Model.h"
#include "Texture2D.h"
#include "TextureCache.h"

void MaterialLibrary::loadForModel(
    const Model& model,
    const std::filesystem::path& modelDirectory,
    TextureCache& textureCache
) {
    destroy();

    for (const std::string& textureName : model.diffuseTextureNames()) {
        if (textureName.empty()
            || diffuseTextures_.find(textureName) != diffuseTextures_.end()) {
            continue;
        }

        const Texture2D* texture = textureCache.load(
            modelDirectory / textureName,
            true
        );
        if (texture != nullptr) {
            diffuseTextures_.emplace(textureName, texture);
        }
    }
}

GLuint MaterialLibrary::diffuseTextureId(
    const std::string& textureName,
    GLuint fallbackTexture
) const {
    const auto texture = diffuseTextures_.find(textureName);
    return texture == diffuseTextures_.end()
        ? fallbackTexture
        : texture->second->id();
}

void MaterialLibrary::destroy() {
    diffuseTextures_.clear();
}

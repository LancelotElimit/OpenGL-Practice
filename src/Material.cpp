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

        const auto embedded = model.embeddedImages().find(textureName);
        const Texture2D* texture = embedded == model.embeddedImages().end() ?
            textureCache.load(modelDirectory / textureName, true) :
            textureCache.loadEmbedded(modelDirectory.generic_string() + "/" +
                std::to_string(reinterpret_cast<std::uintptr_t>(&model)) + "/" + textureName,
                embedded->second.bytes.data(), embedded->second.bytes.size(),
                embedded->second.width, embedded->second.height);
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

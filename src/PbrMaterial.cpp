#include "PbrMaterial.h"

#include "Texture2D.h"
#include "TextureCache.h"

bool PbrMaterial::load(
    const std::filesystem::path& albedoPath,
    const std::filesystem::path& normalPath,
    const std::filesystem::path& roughnessPath,
    const std::filesystem::path& aoPath,
    TextureCache& textureCache
) {
    destroy();
    albedo_ = textureCache.load(albedoPath, true);
    normal_ = textureCache.load(normalPath, true);
    roughness_ = textureCache.load(roughnessPath, true);
    ao_ = textureCache.load(aoPath, true);
    valid_ = albedo_ != nullptr
        && normal_ != nullptr
        && roughness_ != nullptr
        && ao_ != nullptr;
    if (!valid_) {
        destroy();
    }
    return valid_;
}

bool PbrMaterial::valid() const {
    return valid_;
}

void PbrMaterial::bind(
    GLuint albedoUnit,
    GLuint normalUnit,
    GLuint roughnessUnit,
    GLuint aoUnit
) const {
    albedo_->bind(albedoUnit);
    normal_->bind(normalUnit);
    roughness_->bind(roughnessUnit);
    ao_->bind(aoUnit);
}

void PbrMaterial::destroy() {
    valid_ = false;
    albedo_ = nullptr;
    normal_ = nullptr;
    roughness_ = nullptr;
    ao_ = nullptr;
}

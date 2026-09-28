#include "TextureCache.h"

#include <system_error>
#include <utility>

const Texture2D* TextureCache::load(
    const std::filesystem::path& path,
    bool flipVertically
) {
    if (path.empty()) {
        return nullptr;
    }

    std::error_code error;
    std::filesystem::path normalized = std::filesystem::absolute(path, error);
    if (error) {
        normalized = path;
    }
    normalized = normalized.lexically_normal();
    const std::string key = normalized.generic_string()
        + (flipVertically ? "|flip" : "|no-flip");

    const auto existing = textures_.find(key);
    if (existing != textures_.end()) {
        return &existing->second;
    }

    Texture2D texture;
    if (!texture.loadRGBA(normalized, flipVertically)) {
        return nullptr;
    }
    const auto [inserted, success] = textures_.emplace(
        key,
        std::move(texture)
    );
    return success ? &inserted->second : nullptr;
}

std::size_t TextureCache::size() const {
    return textures_.size();
}

void TextureCache::destroy() {
    textures_.clear();
}

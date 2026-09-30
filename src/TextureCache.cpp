#include "TextureCache.h"

#include <system_error>
#include <utility>
#include <vector>
#include <algorithm>

const Texture2D* TextureCache::loadEmbedded(const std::string& sourceKey, const unsigned char* bytes,
                                           std::size_t size, int width, int height) {
    const std::string key = "embedded|" + sourceKey;
    if (auto it = textures_.find(key); it != textures_.end()) return &it->second;
    Texture2D texture;
    if (!height) {
        if (!texture.loadMemory(bytes, size, true)) return nullptr;
    } else {
        if (width <= 0 || height <= 0 || size != static_cast<std::size_t>(width) * height * 4) return nullptr;
        std::vector<unsigned char> flipped(size);
        const std::size_t row = static_cast<std::size_t>(width) * 4;
        for (int y = 0; y < height; ++y)
            std::copy_n(bytes + y * row, row, flipped.data() + (height - y - 1) * row);
        texture.createRGBA(width, height, flipped.data());
    }
    return &textures_.emplace(key,std::move(texture)).first->second;
}

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

#pragma once

#include "Texture2D.h"

#include <filesystem>
#include <string>
#include <unordered_map>

// Owns each file-backed texture once and shares it between materials.
class TextureCache {
public:
    const Texture2D* load(
        const std::filesystem::path& path,
        bool flipVertically
    );
    std::size_t size() const;
    void destroy();

private:
    std::unordered_map<std::string, Texture2D> textures_;
};

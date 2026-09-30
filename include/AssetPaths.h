#pragma once

#include <filesystem>
void setProjectAssetRoot(const std::filesystem::path& root);

std::filesystem::path findAssetPath(
    const std::filesystem::path& relativePath
);

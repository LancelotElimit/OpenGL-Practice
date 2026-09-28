#pragma once

#include <filesystem>

std::filesystem::path findAssetPath(
    const std::filesystem::path& relativePath
);

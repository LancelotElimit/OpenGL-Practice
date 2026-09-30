#pragma once
#include <filesystem>
#include <string>

enum class AssetCategory { Unknown, Model, Image, Audio, Video };
namespace AssetFormats {
std::string extension(const std::filesystem::path &path);
AssetCategory category(const std::filesystem::path &path);
bool model(const std::filesystem::path &path);
bool gltf(const std::filesystem::path &path);
const char *categoryName(AssetCategory category);
const char *modelExtensions();
} // namespace AssetFormats

#include "AssetFormats.h"
#include <algorithm>
#include <cctype>
#include <string_view>
namespace AssetFormats {
std::string extension(const std::filesystem::path &path) {
    auto result = path.extension().string();
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
bool contains(std::string_view list, const std::string &ext) {
    return !ext.empty() && list.find("|" + ext + "|") != std::string_view::npos;
}
AssetCategory category(const std::filesystem::path &path) {
    const auto ext = extension(path);
    if (contains("|.obj|.gltf|.glb|.fbx|.dae|.3ds|.ply|.stl|.off|.x|.dxf|.lwo|.lws|", ext))
        return AssetCategory::Model;
    if (contains("|.png|.jpg|.jpeg|.bmp|.tga|.gif|.psd|.hdr|.pic|.ppm|.pgm|.pnm|", ext))
        return AssetCategory::Image;
    if (contains("|.wav|.mp3|.wma|.aac|.m4a|.flac|", ext))
        return AssetCategory::Audio;
    if (contains("|.mp4|.m4v|.mov|.avi|.wmv|.asf|.mkv|", ext))
        return AssetCategory::Video;
    return AssetCategory::Unknown;
}
bool model(const std::filesystem::path &path) { return category(path) == AssetCategory::Model; }
bool gltf(const std::filesystem::path &path) {
    const auto ext = extension(path);
    return ext == ".gltf" || ext == ".glb";
}
const char *categoryName(AssetCategory value) {
    switch (value) {
    case AssetCategory::Model:
        return "Model";
    case AssetCategory::Image:
        return "Image";
    case AssetCategory::Audio:
        return "Audio";
    case AssetCategory::Video:
        return "Video";
    default:
        return "Other";
    }
}
const char *modelExtensions() {
    return "OBJ, glTF, GLB, FBX, DAE, 3DS, PLY, STL, OFF, X, DXF, LWO/LWS";
}
} // namespace AssetFormats

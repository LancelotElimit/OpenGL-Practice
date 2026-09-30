#include "AssetPaths.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <vector>
namespace { std::filesystem::path projectAssetRoot; }
void setProjectAssetRoot(const std::filesystem::path& root) { projectAssetRoot=root; }

std::filesystem::path findAssetPath(
    const std::filesystem::path& relativePath
) {
    if (!projectAssetRoot.empty() && !relativePath.empty() && !relativePath.is_absolute() && *relativePath.begin()=="assets") {
        std::filesystem::path suffix;
        for(auto it=++relativePath.begin();it!=relativePath.end();++it) suffix/=*it;
        const auto candidate=projectAssetRoot/suffix;
        return std::filesystem::exists(candidate) ? candidate : std::filesystem::path{};
    }
    std::vector<std::filesystem::path> searchRoots;
    searchRoots.push_back(std::filesystem::current_path());

#ifdef _WIN32
    std::wstring executablePath(32768, L'\0');
    const DWORD pathLength = GetModuleFileNameW(
        nullptr,
        executablePath.data(),
        static_cast<DWORD>(executablePath.size())
    );
    if (pathLength > 0 && pathLength < executablePath.size()) {
        executablePath.resize(pathLength);
        searchRoots.push_back(
            std::filesystem::path(executablePath).parent_path()
        );
    }
#endif

    for (auto root : searchRoots) {
        for (int level = 0; level < 10; ++level) {
            const auto candidate = root / relativePath;
            if (std::filesystem::exists(candidate)) {
                return std::filesystem::absolute(candidate);
            }

            const auto parent = root.parent_path();
            if (parent == root) {
                break;
            }
            root = parent;
        }
    }

    return {};
}

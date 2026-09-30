#pragma once
#include <filesystem>
#include <string>
struct GLFWwindow;
struct ProjectDialogResult {
    std::filesystem::path path;
    std::string error;
};
// Empty path and error means the user cancelled; paths retain native Unicode.
ProjectDialogResult chooseProjectPath(GLFWwindow *owner, const std::filesystem::path &initial,
                                      bool folder, bool chinese);

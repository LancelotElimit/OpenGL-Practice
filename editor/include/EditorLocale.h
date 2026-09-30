#pragma once
#include <filesystem>
#include <string_view>

enum class EditorLanguage { English, Chinese };

// Editor-only localization: never translate asset identifiers or stored scene data.
namespace EditorLocale {
EditorLanguage language();
void setLanguage(EditorLanguage language);
const char *text(const char *key);
// Visible translation + ###original key preserves ImGui IDs across language changes.
// Returned pointers remain valid for the process lifetime.
const char *label(const char *key);
EditorLanguage loadPreference(const std::filesystem::path &path);
bool savePreference(const std::filesystem::path &path, EditorLanguage language);
} // namespace EditorLocale

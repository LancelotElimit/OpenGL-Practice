#include "EditorLocale.h"
#include "EditorTheme.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

void check(bool ok, const char *message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
std::vector<std::string> formats(const std::string &text) {
    static const std::regex format(
        R"(%(?:[-+#0 ]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn]|%))");
    std::vector<std::string> result;
    for (std::sregex_iterator it(text.begin(), text.end(), format), end; it != end; ++it)
        result.push_back(it->str());
    return result;
}
int main() {
    const char *keys[] = {
        "File",
        "Open Project...",
        "Save scene",
        "Edit",
        "Undo",
        "Redo",
        "Window",
        "Hierarchy",
        "Inspector",
        "Resource Browser",
        "Output",
        "Profiler",
        "2D Smoke Lab",
        "3D Water Lab",
        "Reset workspace layout",
        "Play",
        "Editing",
        "Resume",
        "Pause",
        "Step",
        "Stop",
        "Paused",
        "Playing",
        "Unsaved",
        "Open Project",
        "Project descriptor (.lancelot)",
        "Open",
        "Cancel",
        "Save your scene before switching projects.",
        "Scene View",
        "Move [Q]",
        "Rotate [W]",
        "Scale [E]",
        "Click view for WASD",
        "RMB + WASD",
        "F4: return to editor",
        "Name",
        "Instance ID: %u",
        "Visible",
        "Parent",
        "Scene root",
        "Parent rejected: cycle or unsupported shear.",
        "Transform / local to parent",
        "Position",
        "Rotation (deg)",
        "Scale",
        "Local axes",
        "Snap",
        "Move step",
        "Angle step",
        "Scale step",
        "Focus selected [F]",
        "C++ Script",
        "Behaviour",
        "None",
        "Script enabled",
        "Main character",
        "Move speed",
        "Face movement",
        "Follow camera",
        "Camera offset",
        "Runtime is read-only. Stop to edit; runtime changes are discarded.",
        "Scripts are compiled C++. Assign one main character, then Play and click Scene View.",
        "Model component",
        "Asset: %s",
        "Project default",
        "Drop resource here to bind",
        "Override material",
        "Metallic",
        "Roughness",
        "Play animation",
        "Animation speed",
        "Clip",
        "Render water",
        "Pause simulation",
        "Open water controls",
        "Simulate smoke",
        "Scene opacity",
        "Open smoke controls",
        "Surface tint",
        "Surface roughness",
        "Light color",
        "Intensity",
        "Field of view",
        "Preview this camera",
        "Sky background",
        "Environment intensity",
        "Rotation changes the sky and IBL orientation. Position and scale do not affect an "
        "infinite sky. HDR resource comes from the project descriptor.",
        "Scene Settings",
        "Camera",
        "Imported OBJ",
        "glTF Scene",
        "Skinned Model",
        "CPU Particles",
        "GPU Particles",
        "2D Smoke",
        "3D Water",
        "Renderer / Post-processing",
        "Bloom (F3)",
        "Exposure",
        "Select scene objects on the left. The scene view renders to an off-screen texture.",
        "Position: %.2f, %.2f, %.2f",
        "Right drag in Scene View + WASD to navigate.",
        "Focus water",
        "Focus origin",
        "Select a model instance in Scene Objects.",
        "Show glTF",
        "Import another model from Assets.",
        "Show skinned models",
        "Select a skinned object to edit its independent animation.",
        "Select a CPU emitter in Scene Objects.",
        "Select a GPU emitter in Scene Objects.",
        "Show Smoke Lab",
        "Open Water Lab",
        "Detailed simulation and material controls are in Water Lab.",
        "Scene Objects",
        "Duplicate subtree",
        "Delete subtree",
        "Delete",
        "Move to scene root",
        "Drop here to move to root",
        "Reparent rejected: cycle, missing parent or non-TRS shear.",
        "Duplicate",
        "Add object",
        "Systems",
        "Select a component to edit its settings.",
        "Inspect GPU particles",
        "Inspect 3D water",
        "MODEL IMPORT",
        "Import model",
        "Supported imports: OBJ, glTF, GLB.",
        "Import animated skin",
        "Bind selected model",
        "Import adds an independent object. Existing model references are preserved. Ctrl+S saves "
        "references.",
        "Cached models: %zu",
        "Add project OBJ",
        "Add project glTF",
        "Root",
        "Up",
        "Filter files / folders",
        "[Folder] ",
        "Selected: %s",
        "Double-click folders. Scene deletion never deletes files.",
        "No assets directory found.",
        "Transform rejected: parent scale creates shear.",
        "Counts include render passes; editor UI is excluded.",
        "Editing: %s",
        "CPU %.2f ms sim  /  %.2f ms surface",
        "Show water",
        "Pause##water",
        "Reset##water",
        "Splash##water",
        "Step once##water",
        "Surface",
        "Particles",
        "Wireframe",
        "Geometry",
        "Pour water",
        "Pour / second",
        "Gravity",
        "Pressure",
        "Viscosity",
        "Surface rebuilds / second",
        "Surface material",
        "Water",
        "Refraction",
        "Fresnel",
        "Normals",
        "Shading",
        "Opacity",
        "Refraction offset",
        "Light absorption",
        "Environment reflection",
        "Shallow-edge foam",
        "Smoke canvas",
        "LMB: smoke   RMB: obstacle",
        "Smoke controls",
        "%d x %d grid",
        "Simulate",
        "Auto plume",
        "Reset smoke",
        "Reset obstacles",
        "Center obstacle",
        "Density",
        "Velocity",
        "Divergence",
        "Obstacles",
        "Field",
        "Brush radius",
        "Source strength",
        "Flow force",
        "Dissipation",
        "Pressure steps",
        "Clear output",
        "Runtime messages and import status",
        "glTF: %s",
        "Particle emitter / local space",
        "Moving this object moves existing particles too. Rotation changes the effect direction; "
        "gravity is local Y.",
        "Reset this emitter",
        "Emit CPU particles",
        "Soft intersection",
        "Sparks",
        "Smoke",
        "Snow",
        "Preset",
        "Emission / s",
        "Lifetime",
        "Speed",
        "Start size",
        "End size",
        "Show GPU effect",
        "Emit",
        "Fountain",
        "GPU slots",
        "Size",
        "CPU Particle Emitter",
        "GPU Particle Emitter",
        "Platform",
        "Point Light",
        "Spot Light",
        "Environment",
        "Editor ready. Right-drag in Scene View to move the camera.",
        "Scene saved.",
        "Redo scene edit.",
        "Undo scene edit.",
        "Microsoft YaHei unavailable; using default font.",
        "Play: runtime copy started. Click Scene View for WASD.",
        "Stopped. Editing scene and camera restored.",
        "WASD active | Esc: pause",
        "FPS %.1f  |  frame %.2f ms",
        "Draw calls %llu  |  triangles %llu",
        "OBJ %zu/%zu  |  textures %zu  |  CPU particles %zu  |  GPU slots %zu",
        "2D smoke %.2f ms / %d passes  |  water %.2f ms / %zu triangles",
        "%zu particles  |  %zu triangles",
        "Unable to save language preference.",
    };
    EditorLocale::setLanguage(EditorLanguage::Chinese);
    const char *chineseWindow = EditorLocale::label("Scene View");
    const auto chineseId = ImHashStr(chineseWindow);
    check(std::string(EditorLocale::text("Scene View")) == "场景视图", "Chinese translation");
    for (auto key : keys) {
        const auto translated = std::string(EditorLocale::text(key));
        check(translated != key, "Chinese dictionary covers every UI key");
        check(formats(key) == formats(translated), "printf arguments keep order and types");
        const auto chineseWidgetId = ImHashStr(EditorLocale::label(key));
        EditorLocale::setLanguage(EditorLanguage::English);
        check(std::string(EditorLocale::text(key)) == key, "English preserves canonical text");
        check(chineseWidgetId == ImHashStr(EditorLocale::label(key)), "widget IDs survive switch");
        EditorLocale::setLanguage(EditorLanguage::Chinese);
    }
    check(std::string(EditorLocale::text("unknown/asset.glb")) == "unknown/asset.glb",
          "unknown identifiers untouched");
    EditorLocale::setLanguage(EditorLanguage::English);
    check(chineseId == ImHashStr(EditorLocale::label("Scene View")), "window docking ID preserved");
    check(std::string(chineseWindow) == "场景视图###Scene View", "cached pointers survive switch");
    check(std::string(EditorLocale::label("Pause##water")) == "Pause###Pause##water",
          "hidden suffix never visible");

    const auto dir = std::filesystem::temp_directory_path() /
                     ("lancelot-locale-test-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    const auto path = dir / "editor_preferences.ini";
    check(EditorLocale::loadPreference(path) == EditorLanguage::Chinese,
          "missing defaults to Chinese");
    check(EditorLocale::savePreference(path, EditorLanguage::English), "save English preference");
    check(EditorLocale::loadPreference(path) == EditorLanguage::English,
          "English survives restart");
    check(EditorLocale::savePreference(path, EditorLanguage::Chinese), "save Chinese preference");
    check(EditorLocale::loadPreference(path) == EditorLanguage::Chinese,
          "Chinese survives restart");
    {
        std::ofstream file(path);
        file << "language=invalid\n";
    }
    check(EditorLocale::loadPreference(path) == EditorLanguage::Chinese,
          "invalid preference fallback");
    check(!EditorLocale::savePreference(dir / "missing" / "prefs.ini", EditorLanguage::English),
          "write failure reported");
    std::filesystem::remove(path);
    std::filesystem::remove(dir);

    ImGui::CreateContext();
    applyEditorTheme();
    const auto &colors = ImGui::GetStyle().Colors;
    check(colors[ImGuiCol_WindowBg].x > .9f, "warm light background");
    check(colors[ImGuiCol_Text].x < .3f, "dark readable text");
    check(colors[ImGuiCol_Button].x > colors[ImGuiCol_Button].z, "amber buttons not blue");
    ImGui::DestroyContext();
    std::cout << "Editor locale/theme tests passed\n";
}

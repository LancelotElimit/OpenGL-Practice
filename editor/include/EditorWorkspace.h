#pragma once

#include "EditorPanels.h"
#include "SceneHistory.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct EditorViewportSize {
    int width = 640;
    int height = 360;
};

struct GLFWwindow;
struct RendererStats;
class Renderer;
class Camera;
class Scene;
class Project;
#include "PlaySession.h"

// Displays renderer diagnostics over the final scene in the GLFW window.
class EditorWorkspace {
  public:
    EditorWorkspace(GLFWwindow *window, Project &project, PlaySession &play);
    ~EditorWorkspace();

    EditorWorkspace(const EditorWorkspace &) = delete;
    EditorWorkspace &operator=(const EditorWorkspace &) = delete;

    bool valid() const;
    bool consumeRuntimeReset() {
        bool result = runtimeReset_;
        runtimeReset_ = false;
        return result;
    }
    EditorViewportSize beginFrame(Renderer &renderer, Scene &scene, bool interactive);
    bool sceneNavigating() const;
    bool gameInputFocused() const { return gameInputFocused_; }
    void runtimeMessage(const std::string &message) { log(message); }
    void draw(Renderer &renderer, Camera &camera, Scene &scene, const RendererStats &stats,
              float fps, float deltaTime, std::size_t cachedTextureCount, bool interactive,
              float &metallic, float &roughness, float &exposure, bool &bloomEnabled);

  private:
    friend struct HierarchyPanel;
    friend struct InspectorPanel;
    friend struct ResourceBrowserPanel;
    friend struct OutputPanel;
    friend struct ProfilerPanel;
    friend struct SmokePanel;
    friend struct WaterPanel;
    friend struct SelectionController;
    SceneHistory history_;
    bool historyInitialized_ = false, runtimeReset_ = false;
    std::string savedScene_;
    void saveScene(Scene &scene);
    void historyAction(Scene &scene, bool redo);
    HierarchyPanel hierarchy_;
    InspectorPanel inspector_;
    ResourceBrowserPanel resources_;
    OutputPanel output_;
    ProfilerPanel profiler_;
    SmokePanel smoke_;
    WaterPanel water_;
    GLFWwindow *window_ = nullptr;
    Project &project_;
    PlaySession &play_;
    bool gameInputFocused_ = false;
    std::array<char, 1024> projectPath_{};
    bool openProjectPopup_ = false;
    void log(const std::string &message);
    bool valid_ = false;
    std::string iniPath_;
    std::string preferencesPath_;
    int pendingLanguage_ = -1;
    bool resetLayout_ = false;
    int startupFrames_ = 0;
    bool sceneNavigating_ = false;
    int selectedEntity_ = 0;
    std::uint32_t selectedObject_ = 0;
    int gizmoOperation_ = 0;
    bool localAxes_ = false;
    bool snapEnabled_ = false;
    float snapMove_ = .25f, snapAngle_ = 15.f, snapScale_ = .1f;
    float sceneLeft_ = 0.0f;
    float sceneTop_ = 0.0f;
    float sceneRight_ = 0.0f;
    float sceneBottom_ = 0.0f;
};

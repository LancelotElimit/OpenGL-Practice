#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>

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
class PlaySession;

// Displays renderer diagnostics over the final scene in the GLFW window.
class DebugPanel {
public:
    DebugPanel(GLFWwindow* window, Project& project, PlaySession& play);
    ~DebugPanel();

    DebugPanel(const DebugPanel&) = delete;
    DebugPanel& operator=(const DebugPanel&) = delete;

    bool valid() const;
    EditorViewportSize beginFrame(Renderer& renderer, Scene& scene, bool interactive);
    bool sceneNavigating() const;
    bool gameInputFocused() const { return gameInputFocused_; }
    void runtimeMessage(const std::string& message) { log(message); }
    void draw(
        Renderer& renderer,
        Camera& camera,
        Scene& scene,
        const RendererStats& stats,
        float fps,
        float deltaTime,
        std::size_t cachedTextureCount,
        bool interactive,
        float& metallic,
        float& roughness,
        float& exposure,
        bool& bloomEnabled
    );

private:
    GLFWwindow* window_ = nullptr;
    Project& project_;
    PlaySession& play_;
    bool gameInputFocused_=false;
    bool showWater_ = false, showSmoke_ = false;
    std::uint32_t activeWater_ = 0, activeSmoke_ = 0;
    std::array<char,1024> projectPath_{};
    bool openProjectPopup_ = false;
    void drawHierarchy(Renderer& renderer, Scene& scene);
    void drawGizmo(Renderer& renderer, Camera& camera, Scene& scene, bool interactive);
    void objectActions(Scene& scene);
    void drawInspector(Renderer& renderer, Camera& camera, Scene& scene, float& metallic,
                       float& roughness, float& exposure, bool& bloomEnabled);
    void drawAssets(Renderer& renderer, Scene& scene);
    void drawConsole(Renderer& renderer);
    void drawProfiler(const RendererStats& stats, float fps, float deltaTime,
                      std::size_t cachedTextureCount);
    void drawSmoke(Renderer& renderer, Scene& scene, bool interactive);
    void drawWater(Renderer& renderer, Scene& scene, Camera& camera, const RendererStats& stats);
    void log(const std::string& message);
    static constexpr std::size_t HistorySize = 120;
    std::array<float, HistorySize> frameTimes_{};
    std::size_t historyOffset_ = 0;
    bool valid_ = false;
    std::array<char, 512> gltfPath_{};
    std::string iniPath_;
    bool resetLayout_ = false;
    int startupFrames_ = 0;
    bool sceneNavigating_ = false;
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showAssets_ = true;
    bool showConsole_ = true;
    bool showProfiler_ = true;
    int selectedEntity_ = 0;
    std::uint32_t selectedObject_ = 0;
    int gizmoOperation_ = 0;
    bool localAxes_ = false;
    bool snapEnabled_ = false;
    float snapMove_ = .25f, snapAngle_ = 15.f, snapScale_ = .1f;
    std::string assetDirectory_;
    std::string selectedAsset_;
    std::array<char, 128> assetSearch_{};
    float sceneLeft_ = 0.0f;
    float sceneTop_ = 0.0f;
    float sceneRight_ = 0.0f;
    float sceneBottom_ = 0.0f;
    std::vector<std::string> messages_;
};

#pragma once

#include <array>
#include <cstddef>
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

// Displays renderer diagnostics over the final scene in the GLFW window.
class DebugPanel {
public:
    explicit DebugPanel(GLFWwindow* window);
    ~DebugPanel();

    DebugPanel(const DebugPanel&) = delete;
    DebugPanel& operator=(const DebugPanel&) = delete;

    bool valid() const;
    EditorViewportSize beginFrame(Renderer& renderer, bool interactive);
    bool sceneNavigating() const;
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
    void drawHierarchy(Renderer& renderer);
    void drawInspector(Renderer& renderer, Camera& camera, Scene& scene, float& metallic,
                       float& roughness, float& exposure, bool& bloomEnabled);
    void drawAssets(Renderer& renderer);
    void drawConsole(Renderer& renderer);
    void drawProfiler(const RendererStats& stats, float fps, float deltaTime,
                      std::size_t cachedTextureCount);
    void drawSmoke(Renderer& renderer, bool interactive);
    void drawWater(Renderer& renderer, Camera& camera, const RendererStats& stats);
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
    float sceneLeft_ = 0.0f;
    float sceneTop_ = 0.0f;
    float sceneRight_ = 0.0f;
    float sceneBottom_ = 0.0f;
    std::vector<std::string> messages_;
};

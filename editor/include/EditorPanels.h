#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
class EditorWorkspace;
class Renderer;
class Camera;
class Scene;
struct RendererStats;
// Views share workspace selection/context, but do not own engine lifetimes.
struct HierarchyPanel {
    bool open=true;
    static void draw(EditorWorkspace& ui, Renderer& renderer, Scene& scene);
};
struct InspectorPanel {
    bool open=true;
    static void draw(EditorWorkspace& ui, Renderer& renderer, Camera& camera, Scene& scene, float& metallic, float& roughness, float& exposure, bool& bloomEnabled);
};
struct ResourceBrowserPanel {
    bool open=true;
    std::array<char,512> path{};
    std::string directory, selected;
    std::array<char,128> filter{};
    static void draw(EditorWorkspace& ui, Renderer& renderer, Scene& scene);
};
struct OutputPanel {
    bool open=true;
    std::vector<std::string> messages;
    static void draw(EditorWorkspace& ui, Renderer& renderer);
};
struct ProfilerPanel {
    bool open=true;
    static constexpr std::size_t HistorySize=120;
    std::array<float,HistorySize> frameTimes{};
    std::size_t offset=0;
    static void draw(EditorWorkspace& ui, const RendererStats& stats, float fps, float deltaTime, std::size_t cachedTextureCount);
};
struct SmokePanel {
    bool open=false;
    std::uint32_t activeObject=0;
    static void draw(EditorWorkspace& ui, Renderer& renderer, Scene& scene, bool interactive);
};
struct WaterPanel {
    bool open=false;
    std::uint32_t activeObject=0;
    static void draw(EditorWorkspace& ui, Renderer& renderer, Scene& scene, Camera& camera, const RendererStats& stats);
};
struct SelectionController {
    static void draw(EditorWorkspace& ui, Renderer& renderer, Camera& camera, Scene& scene, bool interactive);
    static void actions(EditorWorkspace& ui, Scene& scene);
};

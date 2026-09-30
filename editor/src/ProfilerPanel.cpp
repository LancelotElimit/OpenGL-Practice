#include "EditorWorkspace.h"
#include "PlaySession.h"
#include "AssetPaths.h"
#include "Renderer.h"
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Scene.h"
#include "Project.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include "PanelControls.h"
using namespace EditorControls;

void ProfilerPanel::draw(EditorWorkspace& ui, const RendererStats& stats, float fps, float deltaTime, std::size_t cachedTextureCount) {
    if (!ui.profiler_.open) return;
    if (ImGui::Begin("Profiler", &ui.profiler_.open)) {
        ImGui::Text("FPS %.1f  |  frame %.2f ms", fps, deltaTime * 1000.0f);
        ImGui::PlotLines("##frametimes", ui.profiler_.frameTimes.data(),
            static_cast<int>(ProfilerPanel::HistorySize), static_cast<int>(ui.profiler_.offset),
            nullptr, 0.0f, 50.0f,
            ImVec2(std::max(120.0f, ImGui::GetContentRegionAvail().x), 54.0f));
        ImGui::Text("Draw calls %llu  |  triangles %llu",
            static_cast<unsigned long long>(stats.drawCalls),
            static_cast<unsigned long long>(stats.submittedTriangles));
        ImGui::Text("OBJ %zu/%zu  |  textures %zu  |  CPU particles %zu  |  GPU slots %zu",
            stats.visibleInstances, stats.totalInstances, cachedTextureCount,
            stats.liveParticles, stats.gpuParticleSlots);
        ImGui::Text("2D smoke %.2f ms / %d passes  |  water %.2f ms / %zu triangles",
            stats.smokeUpdateMs, stats.smokePasses,
            stats.fluidUpdateMs, stats.fluidTriangles);
        ImGui::TextDisabled("Counts include render passes; editor UI is excluded.");
    }
    ImGui::End();
}

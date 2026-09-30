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

void SmokePanel::draw(EditorWorkspace& ui, Renderer& renderer, Scene& scene, bool interactive) {
    if(!ui.smoke_.open) return;
    auto* object=scene.find(ui.smoke_.activeObject);
    if(!object || object->kind!=SceneObjectKind::Smoke) {
        for(const auto& candidate:scene.objects()) if(candidate.kind==SceneObjectKind::Smoke) { object=scene.find(candidate.id); break; }
    }
    if(!object || object->kind!=SceneObjectKind::Smoke) { ui.smoke_.open=false; return; }
    ui.smoke_.activeObject=object->id;
    auto& smoke=object->smokeSettings();
    auto& runtime=renderer.smoke2D(object->id);
    if (ImGui::Begin("2D Smoke Lab", &ui.smoke_.open)) {
        ImGui::Text("Editing: %s",object->name.c_str());
        const ImGuiIO& io = ImGui::GetIO();
        const float canvasWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.34f,
                                             140.0f, 300.0f);
        if (ImGui::BeginChild("Smoke canvas", ImVec2(canvasWidth + 22.0f, 0.0f),
                              ImGuiChildFlags_Borders)) {
            const float size = std::max(80.0f, std::min(canvasWidth,
                ImGui::GetContentRegionAvail().y - 44.0f));
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(
                             runtime.displayTexture())),
                         ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
            if (interactive && ImGui::IsItemHovered()) {
                const ImVec2 topLeft = ImGui::GetItemRectMin();
                const glm::vec2 uv = glm::clamp(glm::vec2(
                    (io.MousePos.x - topLeft.x) / size,
                    1.0f - (io.MousePos.y - topLeft.y) / size),
                    glm::vec2(0.0f), glm::vec2(1.0f));
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const glm::vec2 motion(io.MouseDelta.x / size,
                                          -io.MouseDelta.y / size);
                    runtime.inject(uv, motion * 28.0f,
                                               smoke.sourceStrength * 0.13f);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                    runtime.paintObstacle(uv, smoke.brushRadius);
            }
            ImGui::TextDisabled("LMB: smoke   RMB: obstacle");
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Smoke controls", ImVec2(0, 0))) {
            ImGui::Text("%d x %d grid", runtime.resolution(), runtime.resolution());
            ImGui::Checkbox("Simulate", &smoke.enabled);
            ImGui::SameLine(); ImGui::Checkbox("Pause", &smoke.paused);
            ImGui::Checkbox("Auto plume", &smoke.autoSource);
            if (ImGui::Button("Reset smoke")) runtime.reset();
            ImGui::SameLine();
            if (ImGui::Button("Reset obstacles"))
                runtime.resetObstacles(smoke.centerObstacle);
            if (ImGui::Checkbox("Center obstacle", &smoke.centerObstacle))
                runtime.resetObstacles(smoke.centerObstacle);
            const char* views[] = {"Density", "Velocity", "Pressure",
                                   "Divergence", "Obstacles"};
            ImGui::Combo("Field", &smoke.viewMode, views, 5);
            slider("Brush radius", smoke.brushRadius, 0.01f, 0.16f);
            slider("Source strength", smoke.sourceStrength, 0.0f, 3.0f);
            slider("Flow force", smoke.force, 0.0f, 3.0f);
            slider("Dissipation", smoke.dissipation, 0.96f, 1.0f);
            sliderInt("Pressure steps", smoke.pressureIterations, 4, 50);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

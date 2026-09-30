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

void OutputPanel::draw(EditorWorkspace& ui, Renderer& renderer) {
    if (!ui.output_.open) return;
    if (ImGui::Begin("Output", &ui.output_.open)) {
        if (ImGui::Button("Clear output")) ui.output_.messages.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("Runtime messages and import status");
        ImGui::Separator();
        for (const std::string& message : ui.output_.messages) ImGui::TextWrapped("%s", message.c_str());
        ImGui::TextDisabled("glTF: %s", renderer.gltfScene().status().c_str());
    }
    ImGui::End();
}

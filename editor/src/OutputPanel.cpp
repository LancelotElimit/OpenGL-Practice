#include "EditorLocale.h"
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

void OutputPanel::draw(EditorWorkspace &ui, Renderer &renderer) {
    if (!ui.output_.open)
        return;
    if (ImGui::Begin(EditorLocale::label("Output"), &ui.output_.open)) {
        if (ImGui::Button(EditorLocale::label("Clear output")))
            ui.output_.messages.clear();
        ImGui::SameLine();
        ImGui::TextDisabled(EditorLocale::text("Runtime messages and import status"));
        ImGui::Separator();
        for (const std::string &message : ui.output_.messages)
            ImGui::TextWrapped("%s", EditorLocale::text(message.c_str()));
        ImGui::TextDisabled(EditorLocale::text("glTF: %s"), renderer.gltfScene().status().c_str());
    }
    ImGui::End();
}

#include "AssetPaths.h"
#include "Camera.h"
#include "EditorWorkspace.h"
#include "PanelControls.h"
#include "PlaySession.h"
#include "Project.h"
#include "Renderer.h"
#include "Scene.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
using namespace EditorControls;

void SelectionController::draw(EditorWorkspace &ui, Renderer &renderer, Camera &camera,
                               Scene &scene, bool interactive) {
    if (!interactive || ui.sceneRight_ <= ui.sceneLeft_ || ui.sceneBottom_ <= ui.sceneTop_)
        return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool hovered = ImGui::IsWindowHovered() && mouse.x >= ui.sceneLeft_ &&
                         mouse.x < ui.sceneRight_ && mouse.y >= ui.sceneTop_ &&
                         mouse.y < ui.sceneBottom_;
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if ((hovered || focused) && !ui.sceneNavigating_ && !ImGui::GetIO().WantTextInput &&
        !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            ui.gizmoOperation_ = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            ui.gizmoOperation_ = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            ui.gizmoOperation_ = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            if (const auto *object = scene.find(ui.selectedObject_))
                camera.lookAt(object->worldPosition() + glm::vec3(0, 1.5f, 4),
                              object->worldPosition());
    }
    const auto view = camera.viewMatrix();
    const auto projection = glm::perspective(
        glm::radians(camera.fieldOfView()),
        (ui.sceneRight_ - ui.sceneLeft_) / (ui.sceneBottom_ - ui.sceneTop_), .1f, 100.f);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(ui.sceneLeft_, ui.sceneTop_, ui.sceneRight_ - ui.sceneLeft_,
                      ui.sceneBottom_ - ui.sceneTop_);
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::Enable(!ui.sceneNavigating_);
    const auto visible = [&renderer](const SceneObject &object) {
        return object.enabledInHierarchy() &&
               (object.kind == SceneObjectKind::Obj ||
                (object.kind == SceneObjectKind::Gltf && renderer.showGltfScene()) ||
                (object.kind == SceneObjectKind::Skinned && renderer.showGltfModel()) ||
                object.kind >= SceneObjectKind::CpuEmitter);
    };
    std::uint32_t markerHit = 0;
    float markerDistance = 1e30f;
    // Small editor markers identify emitter origins, even when emission is off.
    for (const auto &object : scene.objects()) {
        if (!visible(object) || object.kind == SceneObjectKind::Environment ||
            object.kind == SceneObjectKind::Obj || object.kind == SceneObjectKind::Gltf ||
            object.kind == SceneObjectKind::Skinned || object.kind == SceneObjectKind::Platform)
            continue;
        const auto clip = projection * view * glm::vec4(object.worldPosition(), 1);
        if (clip.w <= 0)
            continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1 || ndc.z > 1 || std::abs(ndc.x) > 1 || std::abs(ndc.y) > 1)
            continue;
        const ImVec2 center(ui.sceneLeft_ + (ndc.x + 1) * .5f * (ui.sceneRight_ - ui.sceneLeft_),
                            ui.sceneTop_ + (1 - ndc.y) * .5f * (ui.sceneBottom_ - ui.sceneTop_));
        const auto color = object.kind == SceneObjectKind::CpuEmitter ? IM_COL32(255, 190, 50, 255)
                                                                      : IM_COL32(90, 200, 255, 255);
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(ImVec2(ui.sceneLeft_, ui.sceneTop_),
                           ImVec2(ui.sceneRight_, ui.sceneBottom_), true);
        draw->AddCircleFilled(center, 6, color);
        draw->AddCircle(center, ui.selectedObject_ == object.id ? 10.f : 8.f,
                        IM_COL32(255, 255, 255, 240));
        draw->AddText(ImVec2(center.x + 10, center.y + 5), color, Scene::typeName(object.kind));
        draw->PopClipRect();
        const float dx = mouse.x - center.x, dy = mouse.y - center.y;
        if (dx * dx + dy * dy <= 121 && clip.w < markerDistance) {
            markerHit = object.id;
            markerDistance = clip.w;
        }
    }
    bool gizmoHovered = false;
    if (auto *object = scene.find(ui.selectedObject_); object && visible(*object)) {
        auto transform = object->editorMatrix();
        const ImGuizmo::OPERATION operation = ui.gizmoOperation_ == 0   ? ImGuizmo::TRANSLATE
                                              : ui.gizmoOperation_ == 1 ? ImGuizmo::ROTATE
                                                                        : ImGuizmo::SCALE;
        const float step = std::max(.001f, ui.gizmoOperation_ == 0   ? ui.snapMove_
                                           : ui.gizmoOperation_ == 1 ? ui.snapAngle_
                                                                     : ui.snapScale_);
        const float snap[] = {step, step, step};
        ImGui::GetWindowDrawList()->PushClipRect(ImVec2(ui.sceneLeft_, ui.sceneTop_),
                                                 ImVec2(ui.sceneRight_, ui.sceneBottom_), true);
        if (ImGuizmo::Manipulate(
                glm::value_ptr(view), glm::value_ptr(projection), operation,
                ui.localAxes_ || ui.gizmoOperation_ == 2 ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                glm::value_ptr(transform), nullptr, ui.snapEnabled_ ? snap : nullptr)) {
            if (!scene.setWorldMatrix(object->id, transform))
                ui.log("Transform rejected: parent scale creates shear.");
        }
        ImGui::GetWindowDrawList()->PopClipRect();
        gizmoHovered = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        ImGui::GetWindowDrawList()->AddText(ImVec2(ui.sceneLeft_ + 10, ui.sceneTop_ + 10),
                                            IM_COL32(255, 210, 90, 255), object->name.c_str());
    }
    // Static assets use triangle hits after a bounds broad phase. It is kept
    // separate from gizmo hit testing so clicking an axis never changes selection.
    if (hovered && !ui.sceneNavigating_ && !gizmoHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float x = 2 * (mouse.x - ui.sceneLeft_) / (ui.sceneRight_ - ui.sceneLeft_) - 1;
        const float y = 1 - 2 * (mouse.y - ui.sceneTop_) / (ui.sceneBottom_ - ui.sceneTop_);
        const auto inverse = glm::inverse(projection * view);
        auto nearPoint = inverse * glm::vec4(x, y, -1, 1);
        auto farPoint = inverse * glm::vec4(x, y, 1, 1);
        const auto origin = glm::vec3(nearPoint) / nearPoint.w;
        const auto direction = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);
        float closest = 1e30f;
        ui.selectedObject_ = 0;
        for (const auto &object : scene.objects()) {
            if (!visible(object))
                continue;
            if (object.kind == SceneObjectKind::Environment)
                continue;
            const float distance = object.intersectRay(origin, direction);
            if (distance >= 0 && distance < closest) {
                closest = distance;
                ui.selectedObject_ = object.id;
            }
        }
        if (markerHit)
            ui.selectedObject_ = markerHit;
    }
    SelectionController::actions(ui, scene);
}

void SelectionController::actions(EditorWorkspace &ui, Scene &scene) {
    if (ui.play_.active())
        return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
        ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing() ||
        ui.sceneNavigating_)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
        ui.gizmoOperation_ = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
        ui.gizmoOperation_ = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false))
        ui.gizmoOperation_ = 2;
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        ui.selectedObject_ = scene.duplicate(ui.selectedObject_);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        scene.remove(ui.selectedObject_);
        ui.selectedObject_ = 0;
    }
}

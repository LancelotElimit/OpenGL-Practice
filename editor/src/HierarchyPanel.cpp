#include "EditorLocale.h"
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
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
using namespace EditorControls;

void HierarchyPanel::draw(EditorWorkspace &ui, Renderer &renderer, Scene &scene) {
    if (!ui.hierarchy_.open)
        return;
    if (ImGui::Begin(EditorLocale::label("Hierarchy"), &ui.hierarchy_.open)) {
        if (ui.selectedObject_ && !scene.find(ui.selectedObject_))
            ui.selectedObject_ = 0;
        if (ImGui::TreeNodeEx(EditorLocale::label("Scene Objects"),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::uint32_t erase = 0, copy = 0;
            std::uint32_t dragged = 0, target = 0;
            bool reparent = false;
            std::function<void(std::uint32_t)> drawChildren = [&](std::uint32_t parent) {
                for (const auto &object : scene.objects()) {
                    if (object.parent != parent)
                        continue;
                    ImGui::PushID(static_cast<int>(object.id));
                    const bool hasChildren =
                        std::any_of(scene.objects().begin(), scene.objects().end(),
                                    [&](const auto &child) { return child.parent == object.id; });
                    auto flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                    if (!hasChildren)
                        flags |= ImGuiTreeNodeFlags_Leaf;
                    if (ui.selectedObject_ == object.id)
                        flags |= ImGuiTreeNodeFlags_Selected;
                    const bool opened = ImGui::TreeNodeEx(object.name.c_str(), flags);
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        ui.selectedObject_ = object.id;
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("SCENE_OBJECT", &object.id, sizeof(object.id));
                        ImGui::TextUnformatted(object.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
                            dragged = *static_cast<const std::uint32_t *>(payload->Data);
                            target = object.id;
                            reparent = true;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::BeginPopupContextItem()) {
                        ui.selectedObject_ = object.id;
                        if (ImGui::MenuItem(EditorLocale::label("Duplicate subtree"), "Ctrl+D"))
                            copy = object.id;
                        if (ImGui::MenuItem(EditorLocale::label("Delete subtree"),
                                            EditorLocale::text("Delete")))
                            erase = object.id;
                        if (ImGui::MenuItem(EditorLocale::label("Move to scene root"))) {
                            dragged = object.id;
                            target = 0;
                            reparent = true;
                        }
                        ImGui::EndPopup();
                    }
                    if (opened) {
                        drawChildren(object.id);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            };
            drawChildren(0);
            ImGui::Selectable(EditorLocale::label("Drop here to move to root"), false);
            if (ImGui::BeginDragDropTarget()) {
                if (const auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
                    dragged = *static_cast<const std::uint32_t *>(payload->Data);
                    target = 0;
                    reparent = true;
                }
                ImGui::EndDragDropTarget();
            }
            if (reparent && !scene.setParent(dragged, target))
                ui.log("Reparent rejected: cycle, missing parent or non-TRS shear.");
            if (copy)
                ui.selectedObject_ = scene.duplicate(copy);
            if (erase) {
                scene.remove(erase);
                ui.selectedObject_ = 0;
            }
            ImGui::TreePop();
        }
        if (ImGui::Button(EditorLocale::label("Duplicate")) && ui.selectedObject_)
            ui.selectedObject_ = scene.duplicate(ui.selectedObject_);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::label("Delete")) && ui.selectedObject_) {
            scene.remove(ui.selectedObject_);
            ui.selectedObject_ = 0;
        }
        if (ImGui::Button(EditorLocale::label("Add object")))
            ImGui::OpenPopup("AddEmitter");
        if (ImGui::BeginPopup("AddEmitter")) {
            for (int i = 0; i < static_cast<int>(SceneObjectKind::Count); ++i) {
                const auto kind = static_cast<SceneObjectKind>(i);
                if (ImGui::MenuItem(EditorLocale::label(Scene::typeName(kind))))
                    ui.selectedObject_ = scene.add(kind);
            }
            ImGui::EndPopup();
        }
        SelectionController::actions(ui, scene);
        if (ImGui::TreeNodeEx(EditorLocale::label("Systems"), ImGuiTreeNodeFlags_DefaultOpen)) {
            const char *labels[] = {
                EditorLocale::text("Scene Settings"), EditorLocale::text("Camera"),
                EditorLocale::text("Imported OBJ"),   EditorLocale::text("glTF Scene"),
                EditorLocale::text("Skinned Model"),  EditorLocale::text("CPU Particles"),
                EditorLocale::text("GPU Particles"),  EditorLocale::text("2D Smoke"),
                EditorLocale::text("3D Water")};
            for (int index = 0; index < 9; ++index) {
                if (index >= 2)
                    continue;
                ImGui::PushID(index);
                if (ImGui::Selectable(labels[index],
                                      !ui.selectedObject_ && ui.selectedEntity_ == index)) {
                    ui.selectedObject_ = 0;
                    ui.selectedEntity_ = index;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::text("Select a component to edit its settings."));
        if (ImGui::Button(EditorLocale::label("Inspect GPU particles"))) {
            ui.selectedObject_ = 0;
            for (const auto &object : scene.objects())
                if (object.kind == SceneObjectKind::GpuEmitter) {
                    ui.selectedObject_ = object.id;
                    break;
                }
        }
        if (ImGui::Button(EditorLocale::label("Inspect 3D water"))) {
            for (const auto &object : scene.objects())
                if (object.kind == SceneObjectKind::Water) {
                    ui.selectedObject_ = ui.water_.activeObject = object.id;
                    break;
                }
            ui.water_.open = true;
        }
    }
    ImGui::End();
}

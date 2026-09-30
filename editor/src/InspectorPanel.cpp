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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
using namespace EditorControls;

void InspectorPanel::draw(EditorWorkspace &ui, Renderer &renderer, Camera &camera, Scene &scene,
                          float &metallic, float &roughness, float &exposure, bool &bloomEnabled) {
    if (!ui.inspector_.open)
        return;
    if (ImGui::Begin(EditorLocale::label("Inspector"), &ui.inspector_.open)) {
        if (auto *object = scene.find(ui.selectedObject_)) {
            std::array<char, 256> name{};
            std::strncpy(name.data(), object->name.c_str(), name.size() - 1);
            if (ImGui::InputText(EditorLocale::label("Name"), name.data(), name.size()))
                object->name = name.data();
            ImGui::Text(EditorLocale::text("Instance ID: %u"), object->id);
            ImGui::Checkbox(EditorLocale::label("Visible"), &object->visible);
            const auto *parent = scene.find(object->parent);
            if (ImGui::BeginCombo(EditorLocale::label("Parent"),
                                  parent ? parent->name.c_str()
                                         : EditorLocale::text("Scene root"))) {
                if (ImGui::Selectable(EditorLocale::label("Scene root"), !object->parent))
                    scene.setParent(object->id, 0);
                for (const auto &candidate : scene.objects())
                    if (candidate.id != object->id)
                        if (ImGui::Selectable(candidate.name.c_str(),
                                              object->parent == candidate.id))
                            if (!scene.setParent(object->id, candidate.id))
                                ui.log("Parent rejected: cycle or unsupported shear.");
                ImGui::EndCombo();
            }
            ImGui::SeparatorText(EditorLocale::text("Transform / local to parent"));
            ImGui::DragFloat3(EditorLocale::label("Position"), &object->position.x, .02f);
            ImGui::DragFloat3(EditorLocale::label("Rotation (deg)"), &object->rotation.x, .5f);
            ImGui::DragFloat3(EditorLocale::label("Scale"), &object->scale.x, .01f, .001f, 100.f);
            object->scale = glm::max(object->scale, glm::vec3(.001f));
            ImGui::Checkbox(EditorLocale::label("Local axes"), &ui.localAxes_);
            ImGui::Checkbox(EditorLocale::label("Snap"), &ui.snapEnabled_);
            ImGui::DragFloat(EditorLocale::label("Move step"), &ui.snapMove_, .01f, .001f, 100.f);
            ImGui::DragFloat(EditorLocale::label("Angle step"), &ui.snapAngle_, 1, 1, 90);
            ImGui::DragFloat(EditorLocale::label("Scale step"), &ui.snapScale_, .01f, .001f, 10);
            if (ImGui::Button(EditorLocale::label("Focus selected [F]")))
                camera.lookAt(object->worldPosition() + glm::vec3(0, 1.5f, 4),
                              object->worldPosition());
            ImGui::SeparatorText(EditorLocale::text("C++ Script"));
            if (ImGui::BeginCombo(EditorLocale::label("Behaviour"),
                                  object->script.type.empty() ? EditorLocale::text("None")
                                                              : object->script.type.c_str())) {
                if (ImGui::Selectable(EditorLocale::label("None"), object->script.type.empty())) {
                    object->script.type.clear();
                    object->script.mainCharacter = false;
                }
                for (const auto &[name, factory] : ui.play_.registry().entries())
                    if (ImGui::Selectable(name.c_str(), object->script.type == name))
                        object->script.type = name;
                ImGui::EndCombo();
            }
            if (!object->script.type.empty()) {
                ImGui::Checkbox(EditorLocale::label("Script enabled"), &object->script.enabled);
                bool player = object->script.mainCharacter;
                if (ImGui::Checkbox(EditorLocale::label("Main character"), &player)) {
                    for (const auto &other : scene.objects())
                        scene.find(other.id)->script.mainCharacter = false;
                    object->script.mainCharacter = player;
                }
                slider("Move speed", object->script.moveSpeed, 0, 20);
                ImGui::Checkbox(EditorLocale::label("Face movement"), &object->script.faceMovement);
                ImGui::Checkbox(EditorLocale::label("Follow camera"), &object->script.followCamera);
                ImGui::DragFloat3(EditorLocale::label("Camera offset"),
                                  &object->script.cameraOffset.x, .05f);
            }
            ImGui::TextWrapped(
                ui.play_.active()
                    ? EditorLocale::text(
                          "Runtime is read-only. Stop to edit; runtime changes are discarded.")
                    : EditorLocale::text("Scripts are compiled C++. Assign one main character, "
                                         "then Play and click Scene View."));
            if (auto *model = dynamic_cast<ModelObject *>(object)) {
                ImGui::SeparatorText(EditorLocale::text("Model component"));
                ImGui::TextWrapped(EditorLocale::text("Asset: %s"),
                                   model->model.asset.source.empty()
                                       ? EditorLocale::text("Project default")
                                       : model->model.asset.source.c_str());
                ImGui::Button(EditorLocale::label("Drop resource here to bind"));
                if (ImGui::BeginDragDropTarget()) {
                    if (const auto *payload = ImGui::AcceptDragDropPayload("MODEL_ASSET")) {
                        renderer.assets().bindModel(scene, object->id,
                                                    static_cast<const char *>(payload->Data));
                        ui.log(renderer.assets().status());
                    }
                    ImGui::EndDragDropTarget();
                }
                if (object->kind == SceneObjectKind::Obj) {
                    ImGui::Checkbox(EditorLocale::label("Override material"),
                                    &model->model.overrideMaterial);
                    if (model->model.overrideMaterial) {
                        slider("Metallic", model->model.metallic, 0, 1);
                        slider("Roughness", model->model.roughness, .05f, 1);
                    }
                }
                if (object->kind == SceneObjectKind::Skinned) {
                    ImGui::Checkbox(EditorLocale::label("Play animation"), &model->model.playing);
                    slider("Animation speed", model->model.playbackSpeed, 0, 3);
                    if (auto *animation = renderer.simulations().animation(object->id);
                        animation && animation->animationCount()) {
                        const int index =
                            std::clamp(model->model.clip, 0,
                                       static_cast<int>(animation->animationCount()) - 1);
                        if (ImGui::BeginCombo(EditorLocale::label("Clip"),
                                              animation->animationName(index).c_str())) {
                            for (int i = 0; i < static_cast<int>(animation->animationCount()); ++i)
                                if (ImGui::Selectable(animation->animationName(i).c_str(),
                                                      i == index))
                                    model->model.clip = i;
                            ImGui::EndCombo();
                        }
                    }
                }
            }
            if (object->kind == SceneObjectKind::CpuEmitter ||
                object->kind == SceneObjectKind::GpuEmitter)
                particleControls(renderer, *object);
            if (object->kind == SceneObjectKind::Water) {
                ui.water_.activeObject = object->id;
                ImGui::Checkbox(EditorLocale::label("Render water"),
                                &object->waterSettings().visible);
                ImGui::Checkbox(EditorLocale::label("Pause simulation"),
                                &object->waterSettings().paused);
                if (ImGui::Button(EditorLocale::label("Open water controls")))
                    ui.water_.open = true;
            }
            if (object->kind == SceneObjectKind::Smoke) {
                ui.smoke_.activeObject = object->id;
                ImGui::Checkbox(EditorLocale::label("Simulate smoke"),
                                &object->smokeSettings().enabled);
                slider("Scene opacity", object->smokeSettings().opacity, 0, 1);
                if (ImGui::Button(EditorLocale::label("Open smoke controls")))
                    ui.smoke_.open = true;
            }
            if (auto *platform = dynamic_cast<PlatformObject *>(object)) {
                ImGui::ColorEdit3(EditorLocale::label("Surface tint"), &platform->tint.x);
                slider("Surface roughness", platform->roughness, .05f, 1);
            }
            if (auto *light = dynamic_cast<LightObject *>(object)) {
                ImGui::ColorEdit3(EditorLocale::label("Light color"), &light->settings.color.x);
                slider("Intensity", light->settings.intensity, 0, 30);
            }
            if (auto *sceneCamera = dynamic_cast<CameraObject *>(object)) {
                slider("Field of view", sceneCamera->fov, 20, 100);
                if (ImGui::Button(EditorLocale::label("Preview this camera"))) {
                    const auto direction =
                        glm::vec3(object->editorMatrix() * glm::vec4(0, 0, -1, 0));
                    camera.lookAt(object->worldPosition(), object->worldPosition() + direction);
                    camera.setFieldOfView(sceneCamera->fov);
                }
            }
            if (auto *environment = dynamic_cast<EnvironmentObject *>(object)) {
                ImGui::Checkbox(EditorLocale::label("Sky background"), &environment->settings.sky);
                slider("Environment intensity", environment->settings.intensity, 0, 5);
                ImGui::TextWrapped(EditorLocale::text(
                    "Rotation changes the sky and IBL orientation. Position and scale do not "
                    "affect an infinite sky. HDR resource comes from the project descriptor."));
            }
            ImGui::End();
            return;
        }
        const char *titles[] = {
            EditorLocale::text("Scene Settings"), EditorLocale::text("Camera"),
            EditorLocale::text("Imported OBJ"),   EditorLocale::text("glTF Scene"),
            EditorLocale::text("Skinned Model"),  EditorLocale::text("CPU Particles"),
            EditorLocale::text("GPU Particles"),  EditorLocale::text("2D Smoke"),
            EditorLocale::text("3D Water")};
        ImGui::SeparatorText(titles[std::clamp(ui.selectedEntity_, 0, 8)]);
        switch (ui.selectedEntity_) {
        case 0:
            ImGui::TextUnformatted(EditorLocale::text("Renderer / Post-processing"));
            ImGui::Checkbox(EditorLocale::label("Bloom (F3)"), &bloomEnabled);
            slider("Exposure", exposure, 0.1f, 5.0f);
            ImGui::Separator();
            ImGui::TextWrapped(EditorLocale::text("Select scene objects on the left. The scene "
                                                  "view renders to an off-screen texture."));
            break;
        case 1: {
            const glm::vec3 p = camera.position();
            ImGui::Text(EditorLocale::text("Position: %.2f, %.2f, %.2f"), p.x, p.y, p.z);
            ImGui::TextUnformatted(
                EditorLocale::text("Right drag in Scene View + WASD to navigate."));
            if (ImGui::Button(EditorLocale::label("Focus water")))
                camera.lookAt(glm::vec3(1.15f, 1.0f, 3.7f), glm::vec3(1.15f, 0.25f, 0.0f));
            if (ImGui::Button(EditorLocale::label("Focus origin")))
                camera.lookAt(glm::vec3(0.0f, 1.5f, 4.0f), glm::vec3(0.0f));
            break;
        }
        case 2:
            ImGui::TextDisabled(EditorLocale::text("Select a model instance in Scene Objects."));
            break;
        case 3:
            ImGui::Checkbox(EditorLocale::label("Show glTF"), &renderer.showGltfScene());
            ImGui::TextWrapped("%s", renderer.gltfScene().status().c_str());
            ImGui::TextDisabled(EditorLocale::text("Import another model from Assets."));
            break;
        case 4:
            ImGui::Checkbox(EditorLocale::label("Show skinned models"), &renderer.showGltfModel());
            ImGui::TextDisabled(
                EditorLocale::text("Select a skinned object to edit its independent animation."));
            break;
        case 5: {
            ImGui::TextDisabled(EditorLocale::text("Select a CPU emitter in Scene Objects."));
            break;
        }
        case 6: {
            ImGui::TextDisabled(EditorLocale::text("Select a GPU emitter in Scene Objects."));
            break;
        }
        case 7:
            ImGui::Checkbox(EditorLocale::label("Show Smoke Lab"), &ui.smoke_.open);
            break;
        case 8:
            ImGui::Checkbox(EditorLocale::label("Open Water Lab"), &ui.water_.open);
            ImGui::TextDisabled(
                EditorLocale::text("Detailed simulation and material controls are in Water Lab."));
            break;
        }
    }
    ImGui::End();
}

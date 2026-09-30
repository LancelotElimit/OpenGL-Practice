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

void ResourceBrowserPanel::draw(EditorWorkspace &ui, Renderer &renderer, Scene &scene) {
    if (!ui.resources_.open)
        return;
    if (ImGui::Begin(EditorLocale::label("Resource Browser"), &ui.resources_.open)) {
        ImGui::TextUnformatted(EditorLocale::text("MODEL IMPORT"));
        ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 145.0f));
        ImGui::InputText("##assetpath", ui.resources_.path.data(), ui.resources_.path.size());
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::label("Import model"))) {
            std::filesystem::path path(ui.resources_.path.data());
            if (!path.is_absolute())
                path = ui.project_.assetRoot() / path;
            const auto extension = path.extension().string();
            if (extension == ".obj" || extension == ".gltf" || extension == ".glb") {
                const auto kind =
                    extension == ".obj" ? SceneObjectKind::Obj : SceneObjectKind::Gltf;
                if (auto id = renderer.assets().importModel(scene, kind, path)) {
                    ui.selectedObject_ = id;
                    renderer.showGltfScene() = true;
                }
                ui.log(renderer.assets().status());
            } else
                ui.log("Supported imports: OBJ, glTF, GLB.");
        }
        if (ImGui::Button(EditorLocale::label("Import animated skin"))) {
            std::filesystem::path path(ui.resources_.path.data());
            if (!path.is_absolute())
                path = ui.project_.assetRoot() / path;
            if (auto id = renderer.assets().importModel(scene, SceneObjectKind::Skinned, path)) {
                ui.selectedObject_ = id;
                renderer.showGltfModel() = true;
            }
            ui.log(renderer.assets().status());
        }
        if (ImGui::Button(EditorLocale::label("Bind selected model"))) {
            std::filesystem::path path(ui.resources_.path.data());
            if (!path.is_absolute())
                path = ui.project_.assetRoot() / path;
            renderer.assets().bindModel(scene, ui.selectedObject_, path);
            ui.log(renderer.assets().status());
        }
        ImGui::TextWrapped(
            EditorLocale::text("Import adds an independent object. Existing model references are "
                               "preserved. Ctrl+S saves references."));
        ImGui::Text(EditorLocale::text("Cached models: %zu"), renderer.assets().resourceCount());
        ImGui::TextWrapped("%s", renderer.assets().status().c_str());
        if (ImGui::Button(EditorLocale::label("Add project OBJ")))
            ui.selectedObject_ = scene.add(SceneObjectKind::Obj);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::label("Add project glTF")))
            ui.selectedObject_ = scene.add(SceneObjectKind::Gltf);
        ImGui::Separator();
        const auto assets = ui.project_.assetRoot();
        if (std::filesystem::is_directory(assets)) {
            if (ui.resources_.directory.empty())
                ui.resources_.directory = assets.string();
            const std::filesystem::path current(ui.resources_.directory);
            if (ImGui::Button(EditorLocale::label("Root")))
                ui.resources_.directory = assets.string();
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::label("Up")) && current != assets)
                ui.resources_.directory = current.parent_path().string();
            ImGui::SameLine();
            ImGui::TextUnformatted(current.filename().string().c_str());
            ImGui::InputTextWithHint("##search", EditorLocale::text("Filter files / folders"),
                                     ui.resources_.filter.data(), ui.resources_.filter.size());
            std::error_code error;
            for (const auto &entry : std::filesystem::directory_iterator(current, error)) {
                if (error)
                    break;
                const bool directory = entry.is_directory(error);
                const auto extension = entry.path().extension().string();
                const std::string name = entry.path().filename().string();
                if (name.find(ui.resources_.filter.data()) == std::string::npos)
                    continue;
                const std::string label =
                    std::string(directory ? EditorLocale::text("[Folder] ") : "") + name + "###" +
                    name;
                if (ImGui::Selectable(label.c_str(),
                                      ui.resources_.selected == entry.path().string(),
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    ui.resources_.selected = entry.path().string();
                    if (directory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        ui.resources_.directory = entry.path().string();
                    if (extension == ".glb" || extension == ".gltf" || extension == ".obj") {
                        const std::string relative = entry.path().string();
                        std::strncpy(ui.resources_.path.data(), relative.c_str(),
                                     ui.resources_.path.size() - 1);
                        ui.resources_.path.back() = '\0';
                    }
                }
                if ((extension == ".obj" || extension == ".gltf" || extension == ".glb") &&
                    ImGui::BeginDragDropSource()) {
                    const auto source = entry.path().string();
                    ImGui::SetDragDropPayload("MODEL_ASSET", source.c_str(), source.size() + 1);
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
            ImGui::Separator();
            ImGui::TextWrapped(EditorLocale::text("Selected: %s"), ui.resources_.selected.c_str());
            ImGui::TextDisabled(
                EditorLocale::text("Double-click folders. Scene deletion never deletes files."));
        } else
            ImGui::TextDisabled(EditorLocale::text("No assets directory found."));
    }
    ImGui::End();
}

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

void WaterPanel::draw(EditorWorkspace &ui, Renderer &renderer, Scene &scene, Camera &camera,
                      const RendererStats &stats) {
    if (!ui.water_.open)
        return;
    auto *object = scene.find(ui.water_.activeObject);
    if (!object || object->kind != SceneObjectKind::Water) {
        for (const auto &candidate : scene.objects())
            if (candidate.kind == SceneObjectKind::Water) {
                object = scene.find(candidate.id);
                break;
            }
    }
    if (!object || object->kind != SceneObjectKind::Water) {
        ui.water_.open = false;
        return;
    }
    ui.water_.activeObject = object->id;
    auto &water = object->waterSettings();
    auto &runtime = renderer.fluidSystem(object->id);
    if (ImGui::Begin(EditorLocale::label("3D Water Lab"), &ui.water_.open)) {
        ImGui::Text(EditorLocale::text("Editing: %s"), object->name.c_str());
        ImGui::Text(EditorLocale::text("%zu particles  |  %zu triangles"), stats.fluidParticles,
                    stats.fluidTriangles);
        ImGui::Text(EditorLocale::text("CPU %.2f ms sim  /  %.2f ms surface"),
                    runtime.simulationMilliseconds(), runtime.surfaceMilliseconds());
        ImGui::Checkbox(EditorLocale::label("Show water"), &water.visible);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::label("Pause##water"), &water.paused);
        if (ImGui::Button(EditorLocale::label("Focus water")))
            camera.lookAt(object->position + glm::vec3(0, .75f, 3.7f), object->position);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::label("Reset##water")))
            runtime.reset();
        if (ImGui::Button(EditorLocale::label("Splash##water")))
            runtime.splash();
        ImGui::SameLine();
        ImGui::BeginDisabled(!water.paused);
        if (ImGui::Button(EditorLocale::label("Step once##water")))
            runtime.stepOnce(water);
        ImGui::EndDisabled();
        const char *geometry[] = {EditorLocale::text("Surface"), EditorLocale::text("Particles"),
                                  EditorLocale::text("Wireframe")};
        ImGui::Combo(EditorLocale::label("Geometry"), &water.viewMode, geometry, 3);
        ImGui::Checkbox(EditorLocale::label("Pour water"), &water.pour);
        slider("Pour / second", water.pourRate, 0.0f, 100.0f);
        slider("Gravity", water.gravity, 0.0f, 25.0f);
        slider("Pressure", water.pressure, 0.0f, 50.0f);
        slider("Viscosity", water.viscosity, 0.0f, 1.0f);
        sliderInt("Surface rebuilds / second", water.surfaceHz, 5, 60);
        ImGui::SeparatorText(EditorLocale::text("Surface material"));
        const char *shading[] = {EditorLocale::text("Water"), EditorLocale::text("Refraction"),
                                 EditorLocale::text("Fresnel"), EditorLocale::text("Normals")};
        ImGui::Combo(EditorLocale::label("Shading"), &water.shadingMode, shading, 4);
        slider("Opacity", water.opacity, 0.05f, 1.0f);
        slider("Refraction offset", water.refraction, 0.0f, 0.08f);
        slider("Light absorption", water.absorption, 0.0f, 4.0f);
        slider("Environment reflection", water.reflectivity, 0.0f, 1.0f);
        slider("Roughness", water.roughness, 0.0f, 1.0f);
        slider("Shallow-edge foam", water.foam, 0.0f, 1.0f);
    }
    ImGui::End();
}

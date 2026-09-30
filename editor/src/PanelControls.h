#include "EditorLocale.h"
#pragma once
#include "Renderer.h"
#include "Scene.h"
#include <imgui.h>
namespace EditorControls {
inline void slider(const char *label, float &value, float low, float high) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(EditorLocale::text(label));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##value", &value, low, high, "%.3f");
    ImGui::PopID();
}

inline void sliderInt(const char *label, int &value, int low, int high) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(EditorLocale::text(label));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##value", &value, low, high);
    ImGui::PopID();
}

inline void particleControls(Renderer &renderer, SceneObject &object) {
    ImGui::SeparatorText(EditorLocale::text("Particle emitter / local space"));
    ImGui::TextWrapped(
        EditorLocale::text("Moving this object moves existing particles too. Rotation changes the "
                           "effect direction; gravity is local Y."));
    if (ImGui::Button(EditorLocale::label("Reset this emitter")))
        renderer.resetParticleEmitter(object.id);
    if (object.kind == SceneObjectKind::CpuEmitter) {
        auto &p = object.particleSettings();
        ImGui::Checkbox(EditorLocale::label("Emit CPU particles"), &p.enabled);
        ImGui::Checkbox(EditorLocale::label("Soft intersection"), &p.soft);
        const char *presets[] = {EditorLocale::text("Sparks"), EditorLocale::text("Smoke"),
                                 EditorLocale::text("Snow")};
        if (ImGui::Combo(EditorLocale::label("Preset"), &p.preset, presets, 3)) {
            if (p.preset == 1) {
                p.gravity = .25f;
                p.speed = 1;
                p.startSize = .16f;
                p.endSize = .35f;
            } else if (p.preset == 2) {
                p.gravity = -.1f;
                p.speed = 1;
                p.startSize = .06f;
                p.endSize = .04f;
            } else {
                p.gravity = -2.5f;
                p.speed = 2.2f;
                p.startSize = .12f;
                p.endSize = .02f;
            }
        }
        slider("Emission / s", p.rate, 0, 300);
        slider("Lifetime", p.lifetime, .1f, 8);
        slider("Speed", p.speed, 0, 8);
        slider("Gravity", p.gravity, -10, 10);
        slider("Start size", p.startSize, .01f, .6f);
        slider("End size", p.endSize, .01f, .6f);
    } else {
        auto &g = object.gpuSettings();
        ImGui::Checkbox(EditorLocale::label("Show GPU effect"), &g.visible);
        ImGui::Checkbox(EditorLocale::label("Pause simulation"), &g.paused);
        ImGui::Checkbox(EditorLocale::label("Emit"), &g.emitting);
        const char *presets[] = {EditorLocale::text("Sparks"), EditorLocale::text("Snow"),
                                 EditorLocale::text("Fountain")};
        ImGui::Combo(EditorLocale::label("Preset"), &g.preset, presets, 3);
        sliderInt("GPU slots", g.capacity, 256, GpuParticleSystem::MaxParticles);
        slider("Emission / s", g.emissionRate, 100, 30000);
        slider("Lifetime", g.lifetime, .2f, 8);
        slider("Gravity", g.gravity, -12, 3);
        slider("Size", g.size, .01f, .2f);
    }
}

} // namespace EditorControls

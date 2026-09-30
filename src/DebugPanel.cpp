#include "DebugPanel.h"

#include "AssetPaths.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
#include "Project.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <imm.h>
#endif

namespace {
#ifdef _WIN32
void (*defaultImeCallback)(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData*) = nullptr;
void editorImeCallback(ImGuiContext* context, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
    const auto handle = static_cast<HWND>(viewport->PlatformHandleRaw);
    // GLFW discards VK_PROCESSKEY (IME-consumed keys). Associate an input
    // context only while editing text, leaving Q/W/E available in the viewport.
    if (handle) ImmAssociateContextEx(handle, nullptr, data->WantTextInput ? IACE_DEFAULT : 0);
    if (defaultImeCallback) defaultImeCallback(context, viewport, data);
}
#endif
void slider(const char* label, float& value, float low, float high) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##value", &value, low, high, "%.3f");
    ImGui::PopID();
}

void sliderInt(const char* label, int& value, int low, int high) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##value", &value, low, high);
    ImGui::PopID();
}

void particleControls(Renderer& renderer, SceneObject& object) {
    ImGui::SeparatorText("Particle emitter / local space");
    ImGui::TextWrapped("Moving this object moves existing particles too. Rotation changes the effect direction; gravity is local Y.");
    if (ImGui::Button("Reset this emitter")) renderer.resetParticleEmitter(object.id);
    if (object.kind == SceneObjectKind::CpuEmitter) {
        auto& p = object.particleSettings();
        ImGui::Checkbox("Emit CPU particles", &p.enabled);
        ImGui::Checkbox("Soft intersection", &p.soft);
        const char* presets[] = {"Sparks", "Smoke", "Snow"};
        if (ImGui::Combo("Preset", &p.preset, presets, 3)) {
            if (p.preset == 1) { p.gravity = .25f; p.speed = 1; p.startSize = .16f; p.endSize = .35f; }
            else if (p.preset == 2) { p.gravity = -.1f; p.speed = 1; p.startSize = .06f; p.endSize = .04f; }
            else { p.gravity = -2.5f; p.speed = 2.2f; p.startSize = .12f; p.endSize = .02f; }
        }
        slider("Emission / s", p.rate, 0, 300);
        slider("Lifetime", p.lifetime, .1f, 8);
        slider("Speed", p.speed, 0, 8);
        slider("Gravity", p.gravity, -10, 10);
        slider("Start size", p.startSize, .01f, .6f);
        slider("End size", p.endSize, .01f, .6f);
    } else {
        auto& g = object.gpuSettings();
        ImGui::Checkbox("Show GPU effect", &g.visible);
        ImGui::Checkbox("Pause simulation", &g.paused);
        ImGui::Checkbox("Emit", &g.emitting);
        const char* presets[] = {"Sparks", "Snow", "Fountain"};
        ImGui::Combo("Preset", &g.preset, presets, 3);
        sliderInt("GPU slots", g.capacity, 256, GpuParticleSystem::MaxParticles);
        slider("Emission / s", g.emissionRate, 100, 30000);
        slider("Lifetime", g.lifetime, .2f, 8);
        slider("Gravity", g.gravity, -12, 3);
        slider("Size", g.size, .01f, .2f);
    }
}
} // namespace

DebugPanel::DebugPanel(GLFWwindow* window, Project& project) : window_(window), project_(project) {
    std::strncpy(gltfPath_.data(), "assets/BoxTextured.glb", gltfPath_.size() - 1);
    std::filesystem::path settingsDirectory = std::filesystem::current_path();
#ifdef _WIN32
    if (const char* localData = std::getenv("LOCALAPPDATA"))
        settingsDirectory = std::filesystem::path(localData) / "OpenGLPractice";
#endif
    std::error_code error;
    std::filesystem::create_directories(settingsDirectory, error);
    iniPath_ = (settingsDirectory / "editor_layout.ini").string();
    resetLayout_ = !std::filesystem::exists(iniPath_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    const auto fontPath=std::filesystem::path(std::getenv("WINDIR") ? std::getenv("WINDIR") : "C:/Windows")/"Fonts/msyh.ttc";
    if (std::filesystem::exists(fontPath)) io.FontDefault=io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),20.f);
    if (!io.FontDefault) { io.FontDefault=io.Fonts->AddFontDefault(); log("Microsoft YaHei unavailable; using default font."); }
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = iniPath_.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 20.0f;
    style.FontScaleDpi = 1.0f;
    style.ScaleAllSizes(1.08f);
    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 3.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.118f, 0.137f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.18f, 0.23f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.17f, 0.27f, 0.37f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.37f, 0.51f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.31f, 0.42f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.43f, 0.57f, 1.0f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        return;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return;
    }
    valid_ = true;
#ifdef _WIN32
    defaultImeCallback = ImGui::GetPlatformIO().Platform_SetImeDataFn;
    ImGui::GetPlatformIO().Platform_SetImeDataFn = editorImeCallback;
    ImmAssociateContextEx(glfwGetWin32Window(window_), nullptr, 0);
#endif
    log("Editor ready. Right-drag in Scene View to move the camera.");
}

DebugPanel::~DebugPanel() {
    if (valid_) {
#ifdef _WIN32
        ImmAssociateContextEx(glfwGetWin32Window(window_), nullptr, IACE_DEFAULT);
#endif
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

bool DebugPanel::valid() const { return valid_; }
bool DebugPanel::sceneNavigating() const { return sceneNavigating_; }

void DebugPanel::log(const std::string& message) {
    messages_.push_back(message);
    if (messages_.size() > 128) messages_.erase(messages_.begin());
}

EditorViewportSize DebugPanel::beginFrame(Renderer& renderer, Scene& scene, bool interactive) {
    if (!valid_) return {};
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("LANCELOT  /  EDITOR");
        ImGui::Separator();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Project...")) openProjectPopup_=true;
            if (ImGui::MenuItem("Save scene","Ctrl+S")) log(project_.saveScene(scene)?"Scene saved.":project_.error());
            ImGui::Separator(); ImGui::TextUnformatted(project_.name().c_str());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Resource Browser", nullptr, &showAssets_);
            ImGui::MenuItem("Output", nullptr, &showConsole_);
            ImGui::MenuItem("Profiler", nullptr, &showProfiler_);
            ImGui::MenuItem("2D Smoke Lab", nullptr, &showSmoke_);
            ImGui::MenuItem("3D Water Lab", nullptr, &showWater_);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset workspace layout")) resetLayout_ = true;
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 285.0f);
        ImGui::TextDisabled("F4: editor / fly camera");
        ImGui::EndMainMenuBar();
    }
    if(openProjectPopup_) { ImGui::OpenPopup("Open Project"); openProjectPopup_=false; }
    if(ImGui::BeginPopupModal("Open Project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Project descriptor (.lancelot)");
        ImGui::SetNextItemWidth(580);
        ImGui::InputText("##project",projectPath_.data(),projectPath_.size());
        if(ImGui::Button("Open")) {
            Project candidate;
            Scene validation;
            if(candidate.open(projectPath_.data()) && candidate.loadScene(validation)) {
                project_.requestedOpen=candidate.descriptor(); ImGui::CloseCurrentPopup();
            } else log(candidate.error());
        }
        ImGui::SameLine(); if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("Save your scene before switching projects.");
        ImGui::EndPopup();
    }

    const ImGuiID dockspace = ImGui::DockSpaceOverViewport();
    if (resetLayout_) {
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
        ImGuiID left = 0, right = 0, bottom = 0, center = dockspace;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.19f, &left, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, &right, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, &bottom, &center);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("3D Water Lab", right);
        ImGui::DockBuilderDockWindow("Scene View", center);
        ImGui::DockBuilderDockWindow("2D Smoke Lab", bottom);
        ImGui::DockBuilderDockWindow("Profiler", bottom);
        ImGui::DockBuilderDockWindow("Output", bottom);
        ImGui::DockBuilderDockWindow("Resource Browser", bottom);
        ImGui::DockBuilderFinish(dockspace);
        resetLayout_ = false;
    }

    EditorViewportSize result;
    sceneNavigating_ = false;
    if (ImGui::Begin("Scene View", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const char* modes[] = {"Move [Q]", "Rotate [W]", "Scale [E]"};
        for (int i = 0; i < 3; ++i) {
            if (i) ImGui::SameLine();
            if (ImGui::Selectable(modes[i], gizmoOperation_ == i, 0,
                ImVec2(ImGui::CalcTextSize(modes[i]).x+16, 0)))
                gizmoOperation_ = i;
        }
        ImGui::SameLine();
        ImGui::TextDisabled(interactive ? "RMB + WASD"
                                        : "F4: return to editor");
        ImGui::Separator();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        // Saved dock sizes can collapse the center after a large-to-small resize.
        // Repair once on startup, while still preserving normally sized layouts.
        if (startupFrames_ < 5 && (available.y < 120.0f || available.x < 180.0f)
            && ImGui::GetIO().DisplaySize.y > 400.0f)
            resetLayout_ = true;
        const ImVec2 size(std::max(64.0f, available.x),
                          std::max(64.0f, available.y));
        ImGui::Dummy(size);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        sceneLeft_ = min.x; sceneTop_ = min.y;
        sceneRight_ = max.x; sceneBottom_ = max.y;
        sceneNavigating_ = interactive && ImGui::IsItemHovered()
            && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        result.width = std::max(64, static_cast<int>(std::round(size.x * scale.x)));
        result.height = std::max(64, static_cast<int>(std::round(size.y * scale.y)));
    }
    ImGui::End();
    ++startupFrames_;
    return result;
}

void DebugPanel::drawHierarchy(Renderer& renderer, Scene& scene) {
    if (!showHierarchy_) return;
    if (ImGui::Begin("Hierarchy", &showHierarchy_)) {
        if (selectedObject_ && !scene.find(selectedObject_)) selectedObject_ = 0;
        if (ImGui::TreeNodeEx("Scene Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::uint32_t erase = 0, copy = 0;
            for (const auto& object : scene.objects()) {
                ImGui::PushID(static_cast<int>(object.id));
                if (ImGui::Selectable(object.name.c_str(), selectedObject_ == object.id))
                    selectedObject_ = object.id;
                if (ImGui::BeginPopupContextItem()) {
                    selectedObject_ = object.id;
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) copy = object.id;
                    if (ImGui::MenuItem("Delete from scene", "Delete")) erase = object.id;
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            if (copy) selectedObject_ = scene.duplicate(copy);
            if (erase) { scene.remove(erase); selectedObject_ = 0; }
            ImGui::TreePop();
        }
        if (ImGui::Button("Duplicate") && selectedObject_) selectedObject_ = scene.duplicate(selectedObject_);
        ImGui::SameLine();
        if (ImGui::Button("Delete") && selectedObject_) { scene.remove(selectedObject_); selectedObject_ = 0; }
        if (ImGui::Button("Add object")) ImGui::OpenPopup("AddEmitter");
        if (ImGui::BeginPopup("AddEmitter")) {
            for(int i=0;i<static_cast<int>(SceneObjectKind::Count);++i) {
                const auto kind=static_cast<SceneObjectKind>(i);
                if(ImGui::MenuItem(Scene::typeName(kind))) selectedObject_=scene.add(kind);
            }
            ImGui::EndPopup();
        }
        objectActions(scene);
        if (ImGui::TreeNodeEx("Systems", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* labels[] = {"Scene Settings", "Camera", "Imported OBJ",
                                    "glTF Scene", "Skinned Model", "CPU Particles",
                                    "GPU Particles", "2D Smoke", "3D Water"};
            for (int index = 0; index < 9; ++index) {
                if (index >= 2) continue;
                ImGui::PushID(index);
                if (ImGui::Selectable(labels[index], !selectedObject_ && selectedEntity_ == index)) {
                    selectedObject_ = 0;
                    selectedEntity_ = index;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::TextDisabled("Select a component to edit its settings.");
        if (ImGui::Button("Inspect GPU particles")) {
            selectedObject_ = 0;
            for (const auto& object : scene.objects()) if (object.kind == SceneObjectKind::GpuEmitter) {
                selectedObject_ = object.id; break;
            }
        }
        if (ImGui::Button("Inspect 3D water")) {
            for(const auto& object:scene.objects()) if(object.kind==SceneObjectKind::Water) { selectedObject_=activeWater_=object.id; break; }
            showWater_=true;
        }
    }
    ImGui::End();
}

void DebugPanel::drawInspector(Renderer& renderer, Camera& camera, Scene& scene,
                               float& metallic, float& roughness,
                               float& exposure, bool& bloomEnabled) {
    if (!showInspector_) return;
    if (ImGui::Begin("Inspector", &showInspector_)) {
        if (auto* object = scene.find(selectedObject_)) {
            std::array<char, 256> name{};
            std::strncpy(name.data(), object->name.c_str(), name.size()-1);
            if (ImGui::InputText("Name", name.data(), name.size())) object->name = name.data();
            ImGui::Text("Instance ID: %u", object->id);
            ImGui::Checkbox("Visible", &object->visible);
            ImGui::SeparatorText("Transform / centered pivot");
            ImGui::DragFloat3("Position", &object->position.x, .02f);
            ImGui::DragFloat3("Rotation (deg)", &object->rotation.x, .5f);
            ImGui::DragFloat3("Scale", &object->scale.x, .01f, .001f, 100.f);
            object->scale = glm::max(object->scale, glm::vec3(.001f));
            ImGui::Checkbox("Local axes", &localAxes_);
            ImGui::Checkbox("Snap", &snapEnabled_);
            ImGui::DragFloat("Move step", &snapMove_, .01f, .001f, 100.f);
            ImGui::DragFloat("Angle step", &snapAngle_, 1, 1, 90);
            ImGui::DragFloat("Scale step", &snapScale_, .01f, .001f, 10);
            if (ImGui::Button("Focus selected [F]"))
                camera.lookAt(object->position + glm::vec3(0, 1.5f, 4), object->position);
            if (object->kind == SceneObjectKind::Obj) {
                slider("Metallic (shared material)", metallic, 0, 1);
                slider("Roughness (shared material)", roughness, .05f, 1);
            }
            if (object->kind == SceneObjectKind::Gltf)
                ImGui::Checkbox("Render glTF assets", &renderer.showGltfScene());
            if (object->kind == SceneObjectKind::Skinned) {
                ImGui::Checkbox("Render skinned assets", &renderer.showGltfModel());
                auto& animated = renderer.gltfModel();
                ImGui::Checkbox("Play shared animation", &animated.playing());
                if (animated.animationCount() > 0) {
                    if (ImGui::BeginCombo("Clip", animated.animationName(animated.animationIndex()).c_str())) {
                        for (int i = 0; i < static_cast<int>(animated.animationCount()); ++i)
                            if (ImGui::Selectable(animated.animationName(i).c_str(), animated.animationIndex() == i))
                                animated.setAnimationIndex(i);
                        ImGui::EndCombo();
                    }
                }
            }
            if (object->kind == SceneObjectKind::CpuEmitter || object->kind == SceneObjectKind::GpuEmitter)
                particleControls(renderer, *object);
            if(object->kind==SceneObjectKind::Water) {
                activeWater_=object->id;
                ImGui::Checkbox("Render water",&object->waterSettings().visible);
                ImGui::Checkbox("Pause simulation",&object->waterSettings().paused);
                if(ImGui::Button("Open water controls")) showWater_=true;
            }
            if(object->kind==SceneObjectKind::Smoke) {
                activeSmoke_=object->id;
                ImGui::Checkbox("Simulate smoke",&object->smokeSettings().enabled);
                slider("Scene opacity",object->smokeSettings().opacity,0,1);
                if(ImGui::Button("Open smoke controls")) showSmoke_=true;
            }
            if(auto* platform=dynamic_cast<PlatformObject*>(object)) {
                ImGui::ColorEdit3("Surface tint",&platform->tint.x);
                slider("Surface roughness",platform->roughness,.05f,1);
            }
            if(auto* light=dynamic_cast<LightObject*>(object)) {
                ImGui::ColorEdit3("Light color",&light->settings.color.x);
                slider("Intensity",light->settings.intensity,0,30);
            }
            if(auto* sceneCamera=dynamic_cast<CameraObject*>(object)) {
                slider("Field of view",sceneCamera->fov,20,100);
                if(ImGui::Button("Preview this camera")) {
                    const auto direction=glm::vec3(object->editorMatrix()*glm::vec4(0,0,-1,0));
                    camera.lookAt(object->position,object->position+direction);
                    camera.setFieldOfView(sceneCamera->fov);
                }
            }
            if(auto* environment=dynamic_cast<EnvironmentObject*>(object)) {
                ImGui::Checkbox("Sky background",&environment->settings.sky);
                slider("Environment intensity",environment->settings.intensity,0,5);
                ImGui::TextWrapped("Rotation changes the sky and IBL orientation. Position and scale do not affect an infinite sky. HDR resource comes from the project descriptor.");
            }
            ImGui::End();
            return;
        }
        const char* titles[] = {"Scene Settings", "Camera", "Imported OBJ",
                                "glTF Scene", "Skinned Model", "CPU Particles",
                                "GPU Particles", "2D Smoke", "3D Water"};
        ImGui::SeparatorText(titles[std::clamp(selectedEntity_, 0, 8)]);
        switch (selectedEntity_) {
        case 0:
            ImGui::TextUnformatted("Renderer / Post-processing");
            ImGui::Checkbox("Bloom (F3)", &bloomEnabled);
            slider("Exposure", exposure, 0.1f, 5.0f);
            ImGui::Separator();
            ImGui::TextWrapped("Select scene objects on the left. The scene view renders to an off-screen texture.");
            break;
        case 1: {
            const glm::vec3 p = camera.position();
            ImGui::Text("Position: %.2f, %.2f, %.2f", p.x, p.y, p.z);
            ImGui::TextUnformatted("Right drag in Scene View + WASD to navigate.");
            if (ImGui::Button("Focus water"))
                camera.lookAt(glm::vec3(1.15f, 1.0f, 3.7f),
                              glm::vec3(1.15f, 0.25f, 0.0f));
            if (ImGui::Button("Focus origin"))
                camera.lookAt(glm::vec3(0.0f, 1.5f, 4.0f), glm::vec3(0.0f));
            break;
        }
        case 2: ImGui::TextDisabled("Select a model instance in Scene Objects."); break;
        case 3:
            ImGui::Checkbox("Show glTF", &renderer.showGltfScene());
            ImGui::TextWrapped("%s", renderer.gltfScene().status().c_str());
            ImGui::TextDisabled("Import another model from Assets.");
            break;
        case 4: {
            ImGui::Checkbox("Show skinned model", &renderer.showGltfModel());
            auto& animated = renderer.gltfModel();
            if (animated.animationCount() > 0) {
                int selected = animated.animationIndex();
                if (ImGui::BeginCombo("Clip", animated.animationName(selected).c_str())) {
                    for (int i = 0; i < static_cast<int>(animated.animationCount()); ++i)
                        if (ImGui::Selectable(animated.animationName(i).c_str(), selected == i))
                            animated.setAnimationIndex(i);
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Play animation", &animated.playing());
                slider("Playback speed", animated.playbackSpeed(), 0.1f, 3.0f);
            }
            break;
        }
        case 5: {
            ImGui::TextDisabled("Select a CPU emitter in Scene Objects.");
            break;
        }
        case 6: {
            ImGui::TextDisabled("Select a GPU emitter in Scene Objects.");
            break;
        }
        case 7:
            ImGui::Checkbox("Show Smoke Lab", &showSmoke_);
            break;
        case 8:
            ImGui::Checkbox("Open Water Lab", &showWater_);
            ImGui::TextDisabled("Detailed simulation and material controls are in Water Lab.");
            break;
        }
    }
    ImGui::End();
}

void DebugPanel::objectActions(Scene& scene) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        || ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive()
        || ImGuizmo::IsUsing() || sceneNavigating_) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) gizmoOperation_ = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOperation_ = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOperation_ = 2;
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        selectedObject_ = scene.duplicate(selectedObject_);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        scene.remove(selectedObject_);
        selectedObject_ = 0;
    }
}

void DebugPanel::drawGizmo(Renderer& renderer, Camera& camera, Scene& scene, bool interactive) {
    if (!interactive || sceneRight_ <= sceneLeft_ || sceneBottom_ <= sceneTop_) return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool hovered = ImGui::IsWindowHovered() && mouse.x >= sceneLeft_ && mouse.x < sceneRight_
        && mouse.y >= sceneTop_ && mouse.y < sceneBottom_;
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if ((hovered || focused) && !sceneNavigating_ && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) gizmoOperation_ = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOperation_ = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOperation_ = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            if (const auto* object = scene.find(selectedObject_))
                camera.lookAt(object->position + glm::vec3(0,1.5f,4), object->position);
    }
    const auto view = camera.viewMatrix();
    const auto projection = glm::perspective(glm::radians(camera.fieldOfView()),
        (sceneRight_ - sceneLeft_) / (sceneBottom_ - sceneTop_), .1f, 100.f);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(sceneLeft_, sceneTop_, sceneRight_ - sceneLeft_, sceneBottom_ - sceneTop_);
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::Enable(!sceneNavigating_);
    const auto visible = [&renderer](const SceneObject& object) {
        return object.visible && (object.kind == SceneObjectKind::Obj
            || (object.kind == SceneObjectKind::Gltf && renderer.showGltfScene() && renderer.gltfScene().valid())
            || (object.kind == SceneObjectKind::Skinned && renderer.showGltfModel() && renderer.gltfModel().valid())
            || object.kind >= SceneObjectKind::CpuEmitter);
    };
    std::uint32_t markerHit = 0;
    float markerDistance = 1e30f;
    // Small editor markers identify emitter origins, even when emission is off.
    for (const auto& object : scene.objects()) {
        if (!visible(object) || object.kind==SceneObjectKind::Environment
            || object.kind==SceneObjectKind::Obj || object.kind==SceneObjectKind::Gltf
            || object.kind==SceneObjectKind::Skinned || object.kind==SceneObjectKind::Platform) continue;
        const auto clip = projection * view * glm::vec4(object.position,1);
        if (clip.w <= 0) continue;
        const glm::vec3 ndc = glm::vec3(clip)/clip.w;
        if (ndc.z < -1 || ndc.z > 1 || std::abs(ndc.x) > 1 || std::abs(ndc.y) > 1) continue;
        const ImVec2 center(sceneLeft_ + (ndc.x+1)*.5f*(sceneRight_-sceneLeft_),
            sceneTop_ + (1-ndc.y)*.5f*(sceneBottom_-sceneTop_));
        const auto color = object.kind == SceneObjectKind::CpuEmitter ? IM_COL32(255,190,50,255) : IM_COL32(90,200,255,255);
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(ImVec2(sceneLeft_,sceneTop_), ImVec2(sceneRight_,sceneBottom_), true);
        draw->AddCircleFilled(center, 6, color);
        draw->AddCircle(center, selectedObject_ == object.id ? 10.f : 8.f, IM_COL32(255,255,255,240));
        draw->AddText(ImVec2(center.x+10,center.y+5), color, Scene::typeName(object.kind));
        draw->PopClipRect();
        const float dx = mouse.x-center.x, dy = mouse.y-center.y;
        if (dx*dx+dy*dy <= 121 && clip.w < markerDistance) {
            markerHit = object.id; markerDistance = clip.w;
        }
    }
    bool gizmoHovered = false;
    if (auto* object = scene.find(selectedObject_); object && visible(*object)) {
        auto transform = object->editorMatrix();
        const ImGuizmo::OPERATION operation = gizmoOperation_ == 0 ? ImGuizmo::TRANSLATE :
            gizmoOperation_ == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
        const float step = std::max(.001f, gizmoOperation_ == 0 ? snapMove_ : gizmoOperation_ == 1 ? snapAngle_ : snapScale_);
        const float snap[] = {step, step, step};
        ImGui::GetWindowDrawList()->PushClipRect(ImVec2(sceneLeft_,sceneTop_), ImVec2(sceneRight_,sceneBottom_), true);
        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation,
            localAxes_ || gizmoOperation_ == 2 ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
            glm::value_ptr(transform), nullptr, snapEnabled_ ? snap : nullptr)) {
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), &object->position.x,
                &object->rotation.x, &object->scale.x);
            object->scale = glm::max(object->scale, glm::vec3(.001f));
        }
        ImGui::GetWindowDrawList()->PopClipRect();
        gizmoHovered = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        ImGui::GetWindowDrawList()->AddText(ImVec2(sceneLeft_+10, sceneTop_+10),
            IM_COL32(255,210,90,255), object->name.c_str());
    }
    // Static assets use triangle hits after a bounds broad phase. It is kept
    // separate from gizmo hit testing so clicking an axis never changes selection.
    if (hovered && !sceneNavigating_ && !gizmoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float x = 2 * (mouse.x - sceneLeft_) / (sceneRight_-sceneLeft_) - 1;
        const float y = 1 - 2 * (mouse.y - sceneTop_) / (sceneBottom_-sceneTop_);
        const auto inverse = glm::inverse(projection * view);
        auto nearPoint = inverse * glm::vec4(x,y,-1,1);
        auto farPoint = inverse * glm::vec4(x,y,1,1);
        const auto origin = glm::vec3(nearPoint) / nearPoint.w;
        const auto direction = glm::normalize(glm::vec3(farPoint)/farPoint.w-origin);
        float closest = 1e30f;
        selectedObject_ = 0;
        for (const auto& object : scene.objects()) {
            if (!visible(object)) continue;
            if (object.kind==SceneObjectKind::Environment) continue;
            const float distance = object.intersectRay(origin,direction);
            if (distance >= 0 && distance < closest) { closest = distance; selectedObject_ = object.id; }
        }
        if (markerHit) selectedObject_ = markerHit;
    }
    objectActions(scene);
}

void DebugPanel::drawAssets(Renderer& renderer, Scene& scene) {
    if (!showAssets_) return;
    if (ImGui::Begin("Resource Browser", &showAssets_)) {
        ImGui::TextUnformatted("MODEL IMPORT");
        ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 145.0f));
        ImGui::InputText("##assetpath", gltfPath_.data(), gltfPath_.size());
        ImGui::SameLine();
        if (ImGui::Button("Load glTF / GLB")) {
            std::filesystem::path path(gltfPath_.data());
            if (!path.is_absolute()) {
                const auto found = findAssetPath(path);
                if (!found.empty()) path = found;
            }
            renderer.showGltfScene() = renderer.gltfScene().load(path);
            log(renderer.gltfScene().status());
            if (renderer.showGltfScene()) {
                project_.setAsset("gltf",path);
                if (std::none_of(scene.objects().begin(), scene.objects().end(), [](const auto& o) {
                    return o.kind == SceneObjectKind::Gltf; })) selectedObject_ = scene.add(SceneObjectKind::Gltf);
                const float scale = .75f / std::max(renderer.gltfScene().radius(), .001f);
                scene.configureObject(SceneObjectKind::Gltf,
                    glm::scale(glm::mat4(1), glm::vec3(scale))
                    * glm::translate(glm::mat4(1), -renderer.gltfScene().center()),
                    renderer.gltfScene().center(), renderer.gltfScene().radius());
                scene.setPickingTriangles(SceneObjectKind::Gltf, renderer.gltfScene().pickingTriangles());
            }
        }
        ImGui::TextWrapped("Load replaces the shared glTF asset. Ctrl+S saves its project reference. Instances keep their transforms.");
        if (ImGui::Button("Add OBJ instance")) selectedObject_ = scene.add(SceneObjectKind::Obj);
        ImGui::SameLine();
        if (ImGui::Button("Add glTF instance") && renderer.gltfScene().valid()) {
            selectedObject_ = scene.add(SceneObjectKind::Gltf);
            renderer.showGltfScene() = true;
        }
        if (ImGui::Button("Add skinned instance") && renderer.gltfModel().valid()) {
            selectedObject_ = scene.add(SceneObjectKind::Skinned);
            renderer.showGltfModel() = true;
        }
        ImGui::Separator();
        const auto assets = findAssetPath("assets");
        if (!assets.empty()) {
            if (assetDirectory_.empty()) assetDirectory_ = assets.string();
            const std::filesystem::path current(assetDirectory_);
            if (ImGui::Button("Root")) assetDirectory_ = assets.string();
            ImGui::SameLine();
            if (ImGui::Button("Up") && current != assets) assetDirectory_ = current.parent_path().string();
            ImGui::SameLine(); ImGui::TextUnformatted(current.filename().string().c_str());
            ImGui::InputTextWithHint("##search", "Filter files / folders", assetSearch_.data(), assetSearch_.size());
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(current, error)) {
                if (error) break;
                const bool directory = entry.is_directory(error);
                const auto extension = entry.path().extension().string();
                const std::string name = entry.path().filename().string();
                if (name.find(assetSearch_.data()) == std::string::npos) continue;
                const std::string label = (directory ? "[Folder] " : "") + name;
                if (ImGui::Selectable(label.c_str(), selectedAsset_ == entry.path().string(),
                    ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedAsset_ = entry.path().string();
                    if (directory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        assetDirectory_ = entry.path().string();
                    if (extension == ".glb" || extension == ".gltf") {
                        const std::string relative = entry.path().string();
                        std::strncpy(gltfPath_.data(), relative.c_str(), gltfPath_.size() - 1);
                        gltfPath_.back() = '\0';
                    }
                }
            }
            ImGui::Separator();
            ImGui::TextWrapped("Selected: %s", selectedAsset_.c_str());
            ImGui::TextDisabled("Double-click folders. Scene deletion never deletes files.");
        } else ImGui::TextDisabled("No assets directory found.");
    }
    ImGui::End();
}

void DebugPanel::drawConsole(Renderer& renderer) {
    if (!showConsole_) return;
    if (ImGui::Begin("Output", &showConsole_)) {
        if (ImGui::Button("Clear output")) messages_.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("Runtime messages and import status");
        ImGui::Separator();
        for (const std::string& message : messages_) ImGui::TextWrapped("%s", message.c_str());
        ImGui::TextDisabled("glTF: %s", renderer.gltfScene().status().c_str());
    }
    ImGui::End();
}

void DebugPanel::drawProfiler(const RendererStats& stats, float fps,
                              float deltaTime, std::size_t cachedTextureCount) {
    if (!showProfiler_) return;
    if (ImGui::Begin("Profiler", &showProfiler_)) {
        ImGui::Text("FPS %.1f  |  frame %.2f ms", fps, deltaTime * 1000.0f);
        ImGui::PlotLines("##frametimes", frameTimes_.data(),
            static_cast<int>(HistorySize), static_cast<int>(historyOffset_),
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

void DebugPanel::drawSmoke(Renderer& renderer, Scene& scene, bool interactive) {
    if(!showSmoke_) return;
    auto* object=scene.find(activeSmoke_);
    if(!object || object->kind!=SceneObjectKind::Smoke) {
        for(const auto& candidate:scene.objects()) if(candidate.kind==SceneObjectKind::Smoke) { object=scene.find(candidate.id); break; }
    }
    if(!object || object->kind!=SceneObjectKind::Smoke) { showSmoke_=false; return; }
    activeSmoke_=object->id;
    auto& smoke=object->smokeSettings();
    auto& runtime=renderer.smoke2D(object->id);
    if (ImGui::Begin("2D Smoke Lab", &showSmoke_)) {
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

void DebugPanel::drawWater(Renderer& renderer, Scene& scene, Camera& camera,
                           const RendererStats& stats) {
    if(!showWater_) return;
    auto* object=scene.find(activeWater_);
    if(!object || object->kind!=SceneObjectKind::Water) {
        for(const auto& candidate:scene.objects()) if(candidate.kind==SceneObjectKind::Water) { object=scene.find(candidate.id); break; }
    }
    if(!object || object->kind!=SceneObjectKind::Water) { showWater_=false; return; }
    activeWater_=object->id;
    auto& water=object->waterSettings();
    auto& runtime=renderer.fluidSystem(object->id);
    if (ImGui::Begin("3D Water Lab", &showWater_)) {
        ImGui::Text("Editing: %s",object->name.c_str());
        ImGui::Text("%zu particles  |  %zu triangles", stats.fluidParticles,
                    stats.fluidTriangles);
        ImGui::Text("CPU %.2f ms sim  /  %.2f ms surface",
                    runtime.simulationMilliseconds(), runtime.surfaceMilliseconds());
        ImGui::Checkbox("Show water", &water.visible);
        ImGui::SameLine(); ImGui::Checkbox("Pause##water", &water.paused);
        if (ImGui::Button("Focus water"))
            camera.lookAt(object->position+glm::vec3(0,.75f,3.7f),object->position);
        ImGui::SameLine();
        if (ImGui::Button("Reset##water")) runtime.reset();
        if (ImGui::Button("Splash##water")) runtime.splash();
        ImGui::SameLine();
        ImGui::BeginDisabled(!water.paused);
        if (ImGui::Button("Step once##water")) runtime.stepOnce(water);
        ImGui::EndDisabled();
        const char* geometry[] = {"Surface", "Particles", "Wireframe"};
        ImGui::Combo("Geometry", &water.viewMode, geometry, 3);
        ImGui::Checkbox("Pour water", &water.pour);
        slider("Pour / second", water.pourRate, 0.0f, 100.0f);
        slider("Gravity", water.gravity, 0.0f, 25.0f);
        slider("Pressure", water.pressure, 0.0f, 50.0f);
        slider("Viscosity", water.viscosity, 0.0f, 1.0f);
        sliderInt("Surface rebuilds / second", water.surfaceHz, 5, 60);
        ImGui::SeparatorText("Surface material");
        const char* shading[] = {"Water", "Refraction", "Fresnel", "Normals"};
        ImGui::Combo("Shading", &water.shadingMode, shading, 4);
        slider("Opacity", water.opacity, 0.05f, 1.0f);
        slider("Refraction offset", water.refraction, 0.0f, 0.08f);
        slider("Light absorption", water.absorption, 0.0f, 4.0f);
        slider("Environment reflection", water.reflectivity, 0.0f, 1.0f);
        slider("Roughness", water.roughness, 0.0f, 1.0f);
        slider("Shallow-edge foam", water.foam, 0.0f, 1.0f);
    }
    ImGui::End();
}

void DebugPanel::draw(Renderer& renderer, Camera& camera, Scene& scene,
                      const RendererStats& stats, float fps, float deltaTime,
                      std::size_t cachedTextureCount, bool interactive,
                      float& metallic, float& roughness, float& exposure,
                      bool& bloomEnabled) {
    if (!valid_) return;
    if(ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_S,false))
        log(project_.saveScene(scene)?"Scene saved.":project_.error());
    frameTimes_[historyOffset_] = deltaTime * 1000.0f;
    historyOffset_ = (historyOffset_ + 1) % HistorySize;

    if (ImGui::Begin("Scene View", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (renderer.viewportTexture() != 0) {
            ImGui::GetWindowDrawList()->AddImage(
                ImTextureRef(static_cast<ImTextureID>(renderer.viewportTexture())),
                ImVec2(sceneLeft_, sceneTop_), ImVec2(sceneRight_, sceneBottom_),
                ImVec2(0, 1), ImVec2(1, 0));
        }
        drawGizmo(renderer, camera, scene, interactive);
    }
    ImGui::End();

    drawHierarchy(renderer, scene);
    drawInspector(renderer, camera, scene, metallic, roughness, exposure, bloomEnabled);
    drawAssets(renderer, scene);
    drawConsole(renderer);
    drawProfiler(stats, fps, deltaTime, cachedTextureCount);
    drawSmoke(renderer, scene, interactive);
    drawWater(renderer, scene, camera, stats);

    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0,
        static_cast<GLsizei>(io.DisplaySize.x * io.DisplayFramebufferScale.x),
        static_cast<GLsizei>(io.DisplaySize.y * io.DisplayFramebufferScale.y));
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#include "EditorWorkspace.h"
#include "PlaySession.h"

#include "AssetPaths.h"
#include "Camera.h"
#include "Project.h"
#include "Renderer.h"
#include "Scene.h"
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

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
void (*defaultImeCallback)(ImGuiContext *, ImGuiViewport *, ImGuiPlatformImeData *) = nullptr;
void editorImeCallback(ImGuiContext *context, ImGuiViewport *viewport, ImGuiPlatformImeData *data) {
    const auto handle = static_cast<HWND>(viewport->PlatformHandleRaw);
    // GLFW discards VK_PROCESSKEY (IME-consumed keys). Associate an input
    // context only while editing text, leaving Q/W/E available in the viewport.
    if (handle)
        ImmAssociateContextEx(handle, nullptr, data->WantTextInput ? IACE_DEFAULT : 0);
    if (defaultImeCallback)
        defaultImeCallback(context, viewport, data);
}
#endif
} // namespace

EditorWorkspace::EditorWorkspace(GLFWwindow *window, Project &project, PlaySession &play)
    : window_(window), project_(project), play_(play) {
    const auto defaultModelPath = project_.asset("gltf").string();
    std::strncpy(resources_.path.data(), defaultModelPath.c_str(), resources_.path.size() - 1);
    std::filesystem::path settingsDirectory = std::filesystem::current_path();
#ifdef _WIN32
    if (const char *localData = std::getenv("LOCALAPPDATA"))
        settingsDirectory = std::filesystem::path(localData) / "OpenGLPractice";
#endif
    std::error_code error;
    std::filesystem::create_directories(settingsDirectory, error);
    iniPath_ = (settingsDirectory / "editor_layout.ini").string();
    resetLayout_ = !std::filesystem::exists(iniPath_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    const auto fontPath =
        std::filesystem::path(std::getenv("WINDIR") ? std::getenv("WINDIR") : "C:/Windows") /
        "Fonts/msyh.ttc";
    if (std::filesystem::exists(fontPath))
        io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 20.f);
    if (!io.FontDefault) {
        io.FontDefault = io.Fonts->AddFontDefault();
        log("Microsoft YaHei unavailable; using default font.");
    }
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = iniPath_.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
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

EditorWorkspace::~EditorWorkspace() {
    if (valid_) {
#ifdef _WIN32
        ImmAssociateContextEx(glfwGetWin32Window(window_), nullptr, IACE_DEFAULT);
#endif
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

bool EditorWorkspace::valid() const { return valid_; }
bool EditorWorkspace::sceneNavigating() const { return sceneNavigating_; }

void EditorWorkspace::log(const std::string &message) {
    output_.messages.push_back(message);
    if (output_.messages.size() > 128)
        output_.messages.erase(output_.messages.begin());
}

void EditorWorkspace::saveScene(Scene &scene) {
    if (project_.saveScene(scene)) {
        savedScene_ = Project::serializeScene(scene);
        log("Scene saved.");
    } else
        log(project_.error());
}
void EditorWorkspace::historyAction(Scene &scene, bool redo) {
    if (play_.active() || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing())
        return;
    history_.observe(scene);
    if (redo ? history_.redo(scene) : history_.undo(scene)) {
        runtimeReset_ = true;
        if (!scene.find(selectedObject_))
            selectedObject_ = 0;
        log(redo ? "Redo scene edit." : "Undo scene edit.");
    }
}
EditorViewportSize EditorWorkspace::beginFrame(Renderer &renderer, Scene &scene, bool interactive) {
    if (!valid_)
        return {};
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    if (!historyInitialized_) {
        history_.reset(scene);
        savedScene_ = Project::serializeScene(scene);
        historyInitialized_ = true;
    }
    if (!play_.active() && !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            historyAction(scene, ImGui::GetIO().KeyShift);
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            historyAction(scene, true);
    }

    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("LANCELOT  /  EDITOR");
        ImGui::Separator();
        if (ImGui::BeginMenu("File")) {
            ImGui::BeginDisabled(play_.active());
            if (ImGui::MenuItem("Open Project..."))
                openProjectPopup_ = true;
            if (ImGui::MenuItem("Save scene", "Ctrl+S"))
                saveScene(scene);
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::TextUnformatted(project_.name().c_str());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::BeginDisabled(play_.active());
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, history_.canUndo()))
                historyAction(scene, false);
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history_.canRedo()))
                historyAction(scene, true);
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hierarchy", nullptr, &hierarchy_.open);
            ImGui::MenuItem("Inspector", nullptr, &inspector_.open);
            ImGui::MenuItem("Resource Browser", nullptr, &resources_.open);
            ImGui::MenuItem("Output", nullptr, &output_.open);
            ImGui::MenuItem("Profiler", nullptr, &profiler_.open);
            ImGui::MenuItem("2D Smoke Lab", nullptr, &smoke_.open);
            ImGui::MenuItem("3D Water Lab", nullptr, &water_.open);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset workspace layout"))
                resetLayout_ = true;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (!play_.active()) {
            if (ImGui::Button("Play"))
                play_.command = PlayCommand::Start;
            ImGui::SameLine();
            ImGui::TextDisabled("Editing");
        } else {
            const bool paused = play_.state() == PlayState::Paused;
            if (ImGui::Button(paused ? "Resume" : "Pause"))
                play_.command = paused ? PlayCommand::Resume : PlayCommand::Pause;
            ImGui::SameLine();
            if (paused && ImGui::Button("Step"))
                play_.command = PlayCommand::Step;
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                play_.command = PlayCommand::Stop;
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, .75f, .2f, 1), "%s  %.1fs", paused ? "Paused" : "Playing",
                               play_.time());
        }
        if (!play_.active() && savedScene_ != Project::serializeScene(scene)) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, .7f, .2f, 1), "Unsaved");
        }
        ImGui::EndMainMenuBar();
    }
    if (openProjectPopup_) {
        ImGui::OpenPopup("Open Project");
        openProjectPopup_ = false;
    }
    if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Project descriptor (.lancelot)");
        ImGui::SetNextItemWidth(580);
        ImGui::InputText("##project", projectPath_.data(), projectPath_.size());
        if (ImGui::Button("Open")) {
            Project candidate;
            Scene validation;
            if (candidate.open(projectPath_.data()) && candidate.loadScene(validation)) {
                project_.requestedOpen = candidate.descriptor();
                ImGui::CloseCurrentPopup();
            } else
                log(candidate.error());
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
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
        const char *modes[] = {"Move [Q]", "Rotate [W]", "Scale [E]"};
        ImGui::BeginDisabled(play_.active());
        for (int i = 0; i < 3; ++i) {
            if (i)
                ImGui::SameLine();
            if (ImGui::Selectable(modes[i], gizmoOperation_ == i, 0,
                                  ImVec2(ImGui::CalcTextSize(modes[i]).x + 16, 0)))
                gizmoOperation_ = i;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled(play_.active() ? (gameInputFocused_ ? "WASD active | Esc: pause"
                                                                : "Click view for WASD")
                            : interactive  ? "RMB + WASD"
                                           : "F4: return to editor");
        ImGui::Separator();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        // Saved dock sizes can collapse the center after a large-to-small resize.
        // Repair once on startup, while still preserving normally sized layouts.
        if (startupFrames_ < 5 && (available.y < 120.0f || available.x < 180.0f) &&
            ImGui::GetIO().DisplaySize.y > 400.0f)
            resetLayout_ = true;
        const ImVec2 size(std::max(64.0f, available.x), std::max(64.0f, available.y));
        ImGui::Dummy(size);
        if (!play_.active() && ImGui::BeginDragDropTarget()) {
            if (const auto *payload = ImGui::AcceptDragDropPayload("MODEL_ASSET")) {
                const std::filesystem::path path(static_cast<const char *>(payload->Data));
                const auto kind =
                    path.extension() == ".obj" ? SceneObjectKind::Obj : SceneObjectKind::Gltf;
                if (auto id = renderer.assets().importModel(scene, kind, path)) {
                    selectedObject_ = id;
                    renderer.showGltfScene() = true;
                }
                log(renderer.assets().status());
            }
            ImGui::EndDragDropTarget();
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        sceneLeft_ = min.x;
        sceneTop_ = min.y;
        sceneRight_ = max.x;
        sceneBottom_ = max.y;
        sceneNavigating_ =
            interactive && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        gameInputFocused_ = play_.state() == PlayState::Playing && interactive &&
                            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                            !ImGui::GetIO().WantTextInput;
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        result.width = std::max(64, static_cast<int>(std::round(size.x * scale.x)));
        result.height = std::max(64, static_cast<int>(std::round(size.y * scale.y)));
    }
    ImGui::End();
    ++startupFrames_;
    return result;
}

void EditorWorkspace::draw(Renderer &renderer, Camera &camera, Scene &scene,
                           const RendererStats &stats, float fps, float deltaTime,
                           std::size_t cachedTextureCount, bool interactive, float &metallic,
                           float &roughness, float &exposure, bool &bloomEnabled) {
    if (!valid_)
        return;
    if (!play_.active() && ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_S, false))
        log(project_.saveScene(scene) ? "Scene saved." : project_.error());
    profiler_.frameTimes[profiler_.offset] = deltaTime * 1000.0f;
    profiler_.offset = (profiler_.offset + 1) % ProfilerPanel::HistorySize;

    if (ImGui::Begin("Scene View", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (renderer.viewportTexture() != 0) {
            ImGui::GetWindowDrawList()->AddImage(
                ImTextureRef(static_cast<ImTextureID>(renderer.viewportTexture())),
                ImVec2(sceneLeft_, sceneTop_), ImVec2(sceneRight_, sceneBottom_), ImVec2(0, 1),
                ImVec2(1, 0));
        }
        if (!play_.active())
            SelectionController::draw(*this, renderer, camera, scene, interactive);
    }
    ImGui::End();

    ImGui::BeginDisabled(play_.active());
    HierarchyPanel::draw(*this, renderer, scene);
    InspectorPanel::draw(*this, renderer, camera, scene, metallic, roughness, exposure,
                         bloomEnabled);
    ResourceBrowserPanel::draw(*this, renderer, scene);
    ImGui::EndDisabled();
    OutputPanel::draw(*this, renderer);
    ProfilerPanel::draw(*this, stats, fps, deltaTime, cachedTextureCount);
    ImGui::BeginDisabled(play_.active());
    SmokePanel::draw(*this, renderer, scene, interactive && !play_.active());
    WaterPanel::draw(*this, renderer, scene, camera, stats);
    ImGui::EndDisabled();

    if (!play_.active() && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing() &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        history_.observe(scene);
    scene.refreshTransforms();
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const ImGuiIO &io = ImGui::GetIO();
    glViewport(0, 0, static_cast<GLsizei>(io.DisplaySize.x * io.DisplayFramebufferScale.x),
               static_cast<GLsizei>(io.DisplaySize.y * io.DisplayFramebufferScale.y));
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

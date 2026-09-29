#include "DebugPanel.h"

#include "AssetPaths.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace {
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
} // namespace

DebugPanel::DebugPanel(GLFWwindow* window) {
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = iniPath_.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleDpi = 1.1f;
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
    log("Editor ready. Right-drag in Scene View to move the camera.");
}

DebugPanel::~DebugPanel() {
    if (valid_) {
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

EditorViewportSize DebugPanel::beginFrame(Renderer& renderer, bool interactive) {
    if (!valid_) return {};
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("LANCELOT  /  EDITOR");
        ImGui::Separator();
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Assets", nullptr, &showAssets_);
            ImGui::MenuItem("Output", nullptr, &showConsole_);
            ImGui::MenuItem("Profiler", nullptr, &showProfiler_);
            ImGui::MenuItem("2D Smoke Lab", nullptr, &renderer.smokeSettings().showWindow);
            ImGui::MenuItem("3D Water Lab", nullptr, &renderer.fluidSettings().showWindow);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset workspace layout")) resetLayout_ = true;
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 285.0f);
        ImGui::TextDisabled("F4: editor / fly camera");
        ImGui::EndMainMenuBar();
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
        ImGui::DockBuilderDockWindow("Assets", bottom);
        ImGui::DockBuilderFinish(dockspace);
        resetLayout_ = false;
    }

    EditorViewportSize result;
    sceneNavigating_ = false;
    if (ImGui::Begin("Scene View", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::TextUnformatted("SCENE");
        ImGui::SameLine();
        ImGui::TextDisabled(interactive ? "Right drag + WASD: navigate"
                                        : "F4: return to editor");
        ImGui::Separator();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        // Saved dock sizes can collapse the center after a large-to-small resize.
        // Repair once on startup, while still preserving normally sized layouts.
        if (startupFrames_ < 5 && available.y < 120.0f
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

void DebugPanel::drawHierarchy(Renderer& renderer) {
    if (!showHierarchy_) return;
    if (ImGui::Begin("Hierarchy", &showHierarchy_)) {
        if (ImGui::TreeNodeEx("Inspection Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* labels[] = {"Scene Settings", "Camera", "Imported OBJ",
                                    "glTF Scene", "Skinned Model", "CPU Particles",
                                    "GPU Particles", "2D Smoke", "3D Water"};
            for (int index = 0; index < 9; ++index) {
                ImGui::PushID(index);
                if (ImGui::Selectable(labels[index], selectedEntity_ == index))
                    selectedEntity_ = index;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::TextDisabled("Select a component to edit its settings.");
        if (ImGui::Button("Inspect GPU particles")) selectedEntity_ = 6;
        if (ImGui::Button("Inspect 3D water")) {
            selectedEntity_ = 8;
            renderer.fluidSettings().showWindow = true;
        }
    }
    ImGui::End();
}

void DebugPanel::drawInspector(Renderer& renderer, Camera& camera, Scene& scene,
                               float& metallic, float& roughness,
                               float& exposure, bool& bloomEnabled) {
    if (!showInspector_) return;
    if (ImGui::Begin("Inspector", &showInspector_)) {
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
        case 2:
            ImGui::TextUnformatted("Transform (relative to imported mesh)");
            ImGui::DragFloat3("Position", &scene.modelPosition().x, 0.02f);
            ImGui::DragFloat3("Rotation", &scene.modelRotation().x, 0.5f);
            ImGui::DragFloat3("Scale", &scene.modelScale().x, 0.01f, 0.01f, 10.0f);
            if (ImGui::Button("Reset transform")) {
                scene.modelPosition() = glm::vec3(0.0f);
                scene.modelRotation() = glm::vec3(0.0f);
                scene.modelScale() = glm::vec3(1.0f);
            }
            ImGui::Separator();
            slider("Metallic (Z/X)", metallic, 0.0f, 1.0f);
            slider("Roughness (C/V)", roughness, 0.05f, 1.0f);
            break;
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
            auto& p = renderer.particleSettings();
            ImGui::Checkbox("Emit CPU particles", &p.enabled);
            ImGui::Checkbox("Soft intersection", &p.soft);
            const char* presets[] = {"Sparks", "Smoke", "Snow"};
            if (ImGui::Combo("Preset", &p.preset, presets, 3)) {
                if (p.preset == 1) { p.gravity = 0.25f; p.speed = 1.0f; p.startSize = 0.16f; p.endSize = 0.35f; }
                else if (p.preset == 2) { p.gravity = -0.1f; p.speed = 1.0f; p.startSize = 0.06f; p.endSize = 0.04f; }
                else { p.gravity = -2.5f; p.speed = 2.2f; p.startSize = 0.12f; p.endSize = 0.02f; }
            }
            slider("Emission / s", p.rate, 0.0f, 300.0f);
            slider("Lifetime", p.lifetime, 0.1f, 8.0f);
            slider("Speed", p.speed, 0.0f, 8.0f);
            slider("Gravity", p.gravity, -10.0f, 10.0f);
            slider("Start size", p.startSize, 0.01f, 0.6f);
            slider("End size", p.endSize, 0.01f, 0.6f);
            break;
        }
        case 6: {
            auto& g = renderer.gpuParticleSettings();
            ImGui::Checkbox("Show GPU effect", &g.visible);
            ImGui::SameLine(); ImGui::Checkbox("Pause", &g.paused);
            ImGui::Checkbox("Emit", &g.emitting);
            if (ImGui::Button("Reset GPU particles")) renderer.gpuParticleSystem().reset(g);
            const char* presets[] = {"Sparks", "Snow", "Fountain"};
            ImGui::Combo("Preset", &g.preset, presets, 3);
            sliderInt("GPU slots", g.capacity, 256, GpuParticleSystem::MaxParticles);
            slider("Emission / s", g.emissionRate, 100.0f, 30000.0f);
            slider("Lifetime", g.lifetime, 0.2f, 8.0f);
            slider("Gravity", g.gravity, -12.0f, 3.0f);
            slider("Size", g.size, 0.01f, 0.2f);
            break;
        }
        case 7:
            ImGui::Checkbox("Simulate smoke", &renderer.smokeSettings().enabled);
            ImGui::Checkbox("Show Smoke Lab", &renderer.smokeSettings().showWindow);
            break;
        case 8:
            ImGui::Checkbox("Show water", &renderer.fluidSettings().visible);
            ImGui::Checkbox("Open Water Lab", &renderer.fluidSettings().showWindow);
            ImGui::TextDisabled("Detailed simulation and material controls are in Water Lab.");
            break;
        }
    }
    ImGui::End();
}

void DebugPanel::drawAssets(Renderer& renderer) {
    if (!showAssets_) return;
    if (ImGui::Begin("Assets", &showAssets_)) {
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
        }
        ImGui::TextDisabled("Choose a glTF asset below, or paste a full path.");
        ImGui::Separator();
        const auto assets = findAssetPath("assets");
        if (!assets.empty()) {
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(assets, error)) {
                if (error) break;
                if (!entry.is_regular_file(error)) continue;
                const auto extension = entry.path().extension().string();
                if (extension != ".glb" && extension != ".gltf" && extension != ".obj"
                    && extension != ".png" && extension != ".jpg" && extension != ".hdr")
                    continue;
                const std::string name = entry.path().filename().string();
                if (ImGui::Selectable(name.c_str())) {
                    if (extension == ".glb" || extension == ".gltf") {
                        const std::string relative = "assets/" + name;
                        std::strncpy(gltfPath_.data(), relative.c_str(), gltfPath_.size() - 1);
                        gltfPath_.back() = '\0';
                    }
                }
            }
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

void DebugPanel::drawSmoke(Renderer& renderer, bool interactive) {
    auto& smoke = renderer.smokeSettings();
    if (!smoke.showWindow) return;
    if (ImGui::Begin("2D Smoke Lab", &smoke.showWindow)) {
        const ImGuiIO& io = ImGui::GetIO();
        const float canvasWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.34f,
                                             140.0f, 300.0f);
        if (ImGui::BeginChild("Smoke canvas", ImVec2(canvasWidth + 22.0f, 0.0f),
                              ImGuiChildFlags_Borders)) {
            const float size = std::max(80.0f, std::min(canvasWidth,
                ImGui::GetContentRegionAvail().y - 44.0f));
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(
                             renderer.smoke2D().displayTexture())),
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
                    renderer.smoke2D().inject(uv, motion * 28.0f,
                                               smoke.sourceStrength * 0.13f);
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                    renderer.smoke2D().paintObstacle(uv, smoke.brushRadius);
            }
            ImGui::TextDisabled("LMB: smoke   RMB: obstacle");
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Smoke controls", ImVec2(0, 0))) {
            ImGui::Text("%d x %d grid", renderer.smoke2D().resolution(),
                        renderer.smoke2D().resolution());
            ImGui::Checkbox("Simulate", &smoke.enabled);
            ImGui::SameLine(); ImGui::Checkbox("Pause", &smoke.paused);
            ImGui::Checkbox("Auto plume", &smoke.autoSource);
            if (ImGui::Button("Reset smoke")) renderer.smoke2D().reset();
            ImGui::SameLine();
            if (ImGui::Button("Reset obstacles"))
                renderer.smoke2D().resetObstacles(smoke.centerObstacle);
            if (ImGui::Checkbox("Center obstacle", &smoke.centerObstacle))
                renderer.smoke2D().resetObstacles(smoke.centerObstacle);
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

void DebugPanel::drawWater(Renderer& renderer, Camera& camera,
                           const RendererStats& stats) {
    auto& water = renderer.fluidSettings();
    if (!water.showWindow) return;
    if (ImGui::Begin("3D Water Lab", &water.showWindow)) {
        ImGui::Text("%zu particles  |  %zu triangles", stats.fluidParticles,
                    stats.fluidTriangles);
        ImGui::Text("CPU %.2f ms sim  /  %.2f ms surface",
                    renderer.fluidSystem().simulationMilliseconds(),
                    renderer.fluidSystem().surfaceMilliseconds());
        ImGui::Checkbox("Show water", &water.visible);
        ImGui::SameLine(); ImGui::Checkbox("Pause##water", &water.paused);
        ImGui::Checkbox("Simulate 2D smoke", &renderer.smokeSettings().enabled);
        if (ImGui::Button("Focus water"))
            camera.lookAt(glm::vec3(1.15f, 1.0f, 3.7f),
                          glm::vec3(1.15f, 0.25f, 0.0f));
        ImGui::SameLine();
        if (ImGui::Button("Reset##water")) renderer.fluidSystem().reset();
        if (ImGui::Button("Splash##water")) renderer.fluidSystem().splash();
        ImGui::SameLine();
        ImGui::BeginDisabled(!water.paused);
        if (ImGui::Button("Step once##water")) renderer.fluidSystem().stepOnce(water);
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
    }
    ImGui::End();

    drawHierarchy(renderer);
    drawInspector(renderer, camera, scene, metallic, roughness, exposure, bloomEnabled);
    drawAssets(renderer);
    drawConsole(renderer);
    drawProfiler(stats, fps, deltaTime, cachedTextureCount);
    drawSmoke(renderer, interactive);
    drawWater(renderer, camera, stats);

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

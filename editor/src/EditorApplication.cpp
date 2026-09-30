#include "EditorApplication.h"
#include "EditorInputRouter.h"
#include "EditorWorkspace.h"
#include "EngineApplication.h"
#include "FrameClock.h"
#include "PlaySession.h"
#include "Project.h"
#include "Renderer.h"
#include "Window.h"
#include <iomanip>
#include <sstream>

namespace {
bool applyPlayCommand(PlaySession &play, EngineApplication &engine, EditorInputRouter &input,
                      EditorWorkspace &ui, float &time) {
    const auto command = play.command;
    play.command = PlayCommand::None;
    if (command == PlayCommand::Start) {
        if (play.start(engine.scene(), engine.camera())) {
            engine.renderer().resetSimulation();
            time = 0;
            input.enterEditor(engine.window(), engine.camera());
            ui.runtimeMessage("Play: runtime copy started. Click Scene View for WASD.");
        } else
            ui.runtimeMessage(play.error());
    }
    if (command == PlayCommand::Step) {
        const auto previous = play.time();
        play.step(engine.camera());
        return play.time() > previous;
    }
    if (command == PlayCommand::Pause)
        play.pause();
    if (command == PlayCommand::Resume)
        play.resume();
    if (command == PlayCommand::Stop) {
        play.stop(engine.camera());
        engine.renderer().resetSimulation();
        time = 0;
        ui.runtimeMessage("Stopped. Editing scene and camera restored.");
    }
    return false;
}
class FrameStats {
  public:
    float fps = 0;
    void update(float dt, EngineApplication &engine, const Project &project,
                const RenderOptions &options) {
        elapsed_ += dt;
        ++frames_;
        if (elapsed_ < .25f)
            return;
        fps = elapsed_ > 0 ? frames_ / elapsed_ : 0;
        elapsed_ = 0;
        frames_ = 0;
        const auto &stats = engine.renderer().stats();
        std::ostringstream title;
        title << std::fixed << std::setprecision(1) << project.name() << " | FPS " << fps
              << " | Draws " << stats.drawCalls << " | Triangles " << stats.submittedTriangles
              << " | Instances " << stats.visibleInstances << '/' << stats.totalInstances
              << " | Cached textures " << engine.textureCount() << " | Fluid "
              << stats.fluidUpdateMs << "ms | Metal " << options.metallic << " | Rough "
              << options.roughness;
        engine.window().setTitle(title.str());
    }

  private:
    float elapsed_ = 0;
    int frames_ = 0;
};
} // namespace
int EditorApplication::run(Project &project, const ScriptRegistry &scripts) {
    EngineApplication engine;
    if (!engine.initialize(project))
        return 1;
    // Project switching rebuilds this workspace. Destroy UI/play before the old context.
    PlaySession play(scripts);
    EditorWorkspace ui(engine.window().get(), project, play);
    if (!ui.valid())
        return 1;
    InputSystem platformInput;
    EditorInputRouter input;
    input.enterEditor(engine.window(), engine.camera());
    FrameClock clock(glfwGetTime());
    FrameStats stats;
    RenderOptions options;
    float simulationTime = 0;
    std::string lastScriptError;
    auto &window = engine.window();
    while (!window.shouldClose() && project.requestedOpen.empty()) {
        const float dt = clock.tick(glfwGetTime());
        const bool stepped = applyPlayCommand(play, engine, input, ui, simulationTime);
        const auto rawInput = platformInput.sample(window.get());
        input.globalShortcuts(rawInput, play, window, engine.camera(), options, dt);
        int width = 0, height = 0;
        window.framebufferSize(width, height);
        if (width == 0 || height == 0) {
            window.pollEvents();
            continue;
        }
        auto &scene = play.activeScene(engine.scene());
        const auto viewport = ui.beginFrame(engine.renderer(), scene, input.interactive());
        const auto gameInput = input.route(rawInput, play, ui, engine.camera(), dt);
        play.update(dt, gameInput, engine.camera());
        if (play.active())
            simulationTime = play.time();
        else
            simulationTime += FrameClock::simulationDelta(dt);
        if (!play.error().empty() && play.error() != lastScriptError) {
            ui.runtimeMessage(play.error());
            lastScriptError = play.error();
        }
        if (ui.consumeRuntimeReset())
            engine.renderer().resetSimulation();
        engine.update(scene, stepped ? 1.f / 60.f : dt, simulationTime,
                      stepped || play.state() != PlayState::Paused);
        engine.render(scene, engine.camera(), viewport.width, viewport.height, options,
                      simulationTime);
        stats.update(dt, engine, project, options);
        ui.draw(engine.renderer(), engine.camera(), scene, engine.renderer().stats(), stats.fps, dt,
                engine.textureCount(), input.interactive(), options.metallic, options.roughness,
                options.exposure, options.bloom);
        window.swapBuffers();
        window.pollEvents();
    }
    play.stop(engine.camera());
    return 0;
}

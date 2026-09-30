#include "AssetPaths.h"
#include "EngineApplication.h"
#include "FrameClock.h"
#include "Input.h"
#include "Project.h"
#include "ProjectScripts.h"
#include "RuntimeSession.h"
#include "Window.h"
#include <iostream>
int main(int argc, char **argv) {
    Project project;
    auto path = argc > 1 ? std::filesystem::path(argv[1])
                         : findAssetPath("projects/Sandbox/Sandbox.lancelot");
    if (!project.open(path)) {
        std::cerr << project.error() << '\n';
        return 1;
    }
    EngineApplication engine;
    if (!engine.initialize(project))
        return 1;
    ScriptRegistry scripts;
    registerProjectScripts(scripts);
    RuntimeSession play(scripts);
    if (!play.start(engine.scene(), engine.camera())) {
        std::cerr << play.error() << '\n';
        return 1;
    }
    auto &window = engine.window();
    window.setTitle(project.name() +
                    " | Player | WASD move | Esc pause/resume | close window to quit");
    FrameClock clock(glfwGetTime());
    InputSystem input;
    RenderOptions options;
    while (!window.shouldClose()) {
        window.pollEvents();
        const auto raw = input.sample(window.get());
        const float dt = clock.tick(glfwGetTime());
        if (raw.escape) {
            if (play.state() == PlayState::Paused)
                play.resume();
            else
                play.pause();
        }
        int width = 0, height = 0;
        window.framebufferSize(width, height);
        if (width < 1 || height < 1)
            continue;
        play.update(dt, GameInput{raw.movement}, engine.camera());
        auto &scene = play.activeScene(engine.scene());
        engine.update(scene, dt, play.time(), play.state() == PlayState::Playing);
        engine.render(scene, engine.camera(), width, height, options, play.time());
        engine.present(width, height);
        window.swapBuffers();
    }
    play.stop(engine.camera());
    return 0;
}

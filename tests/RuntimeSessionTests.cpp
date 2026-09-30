#include "ProjectScripts.h"
#include "RuntimeSession.h"
#include <cstdlib>
#include <iostream>
void check(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
int main() {
    ScriptRegistry scripts;
    registerProjectScripts(scripts);
    Scene scene;
    const auto parent = scene.add(SceneObjectKind::Platform),
               actor = scene.add(SceneObjectKind::Obj);
    scene.find(parent)->position = {3, 0, 0};
    scene.find(parent)->scale = {2, 2, 2};
    check(scene.setParent(actor, parent, false), "parent actor");
    scene.refreshTransforms();
    auto &binding = scene.find(actor)->script;
    binding.type = "PlayerController";
    binding.mainCharacter = true;
    binding.moveSpeed = 4;
    Camera camera;
    const auto original = camera.position();
    RuntimeSession runtime(scripts);
    check(runtime.start(scene, camera), "engine-only session starts");
    runtime.update(.05f, {{1, 0}}, camera);
    auto &running = runtime.activeScene(scene);
    check(std::abs(running.find(actor)->worldPosition().x - 3.2f) < .001f,
          "world movement independent of parent scale");
    runtime.pause();
    const auto time = runtime.time();
    runtime.step(camera);
    check(runtime.state() == PlayState::Paused &&
              std::abs(runtime.time() - time - 1.f / 60.f) < .0001f,
          "step advances exactly one frame then pauses");
    runtime.update(1, {{1, 0}}, camera);
    check(runtime.time() == time + 1.f / 60.f, "paused does not advance");
    runtime.stop(camera);
    check(camera.position() == original && scene.find(actor)->position == glm::vec3(0),
          "stop restores authored scene and camera");
    std::cout << "Shared runtime, parented movement and single-step passed without UI.\n";
}

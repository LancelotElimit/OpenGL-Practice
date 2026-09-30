#include "SceneHistory.h"
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
void check(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a - b) < .001f; }
int main() {
    Scene scene;
    const auto parent = scene.add(SceneObjectKind::Platform),
               child = scene.add(SceneObjectKind::Gltf);
    scene.find(parent)->position = {3, 0, 0};
    scene.find(child)->position = {1, 2, 0};
    scene.refreshTransforms();
    const auto before = scene.find(child)->worldPosition();
    check(scene.setParent(child, parent), "valid parent accepted");
    check(near(scene.find(child)->worldPosition(), before), "reparent preserves world transform");
    check(!scene.setParent(parent, child), "cycle rejected");
    scene.find(parent)->visible = false;
    scene.refreshTransforms();
    check(!scene.find(child)->enabledInHierarchy(), "parent visibility propagates");
    scene.find(parent)->visible = true;
    scene.refreshTransforms();
    scene.find(parent)->position.x += 2;
    scene.refreshTransforms();
    check(near(scene.find(child)->worldPosition(), before + glm::vec3(2, 0, 0)),
          "parent moves child");
    check(scene.setWorldMatrix(child, glm::translate(glm::mat4(1), glm::vec3(7, 4, 0))),
          "world gizmo matrix converts to local");
    check(near(scene.find(child)->worldPosition(), {7, 4, 0}), "world-space placement preserved");
    auto &model = dynamic_cast<ModelObject &>(*scene.find(child)).model;
    model.asset.source = "models/first.glb";
    model.clip = 2;
    model.playbackSpeed = .7f;
    SceneHistory history(4);
    history.reset(scene);
    const auto copy = scene.duplicate(parent);
    check(scene.objects().size() == 4, "subtree cloned");
    std::uint32_t copiedChild = 0;
    for (const auto &object : scene.objects())
        if (object.parent == copy)
            copiedChild = object.id;
    check(copiedChild && copiedChild != child, "child ID remapped");
    check(history.observe(scene) && history.undo(scene) && scene.objects().size() == 2,
          "undo clone");
    check(history.redo(scene) && scene.find(copy) && scene.find(copiedChild), "redo preserves IDs");
    check(scene.remove(parent) && !scene.find(child), "delete subtree");
    history.observe(scene);
    check(history.undo(scene) && scene.find(parent) && scene.find(child), "undo subtree deletion");
    scene.find(child)->name = "Changed";
    history.observe(scene);
    check(!history.canRedo(), "new edit invalidates redo");
    std::string error;
    Scene restored;
    check(Project::deserializeScene(Project::serializeScene(scene), restored, error),
          "scene snapshot round-trip");
    check(restored.find(child)->parent == parent && restored.find(copiedChild)->parent == copy,
          "parents persist");
    const auto &component = dynamic_cast<ModelObject &>(*restored.find(child)).model;
    check(component.asset.source == "models/first.glb" && component.clip == 2 &&
              component.playbackSpeed == .7f,
          "model component persists");
    const auto text = Project::serializeScene(restored);
    check(!Project::deserializeScene(
              R"({"version":1,"objects":[{"id":1,"parent":9,"type":"OBJ"}]})", restored, error) &&
              Project::serializeScene(restored) == text,
          "missing parent load transactional");
    check(
        !Project::deserializeScene(
            R"({"version":1,"objects":[{"id":1,"parent":2,"type":"OBJ"},{"id":2,"parent":1,"type":"OBJ"}]})",
            restored, error),
        "serialized cycle rejected");
    check(!Project::deserializeScene(
              R"({"version":1,"objects":[{"id":1,"type":"OBJ"},{"id":1,"type":"OBJ"}]})", restored,
              error),
          "duplicate IDs rejected");
    Scene legacy;
    check(Project::deserializeScene(R"({"version":1,"objects":[{"type":"OBJ"},{"type":"Water"}]})",
                                    legacy, error) &&
              legacy.find(1) && legacy.find(2),
          "old scenes migrate without rewriting input");
    std::cout << "Hierarchy, history, components, IDs and migration passed.\n";
}

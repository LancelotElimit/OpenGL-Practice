#include "Scene.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <cstdlib>

void check(bool result, const char* description) {
    if (!result) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
bool close(float a, float b) { return std::abs(a-b) < .001f; }
int main() {
    Scene scene;
    for(int i=0;i<5;++i) scene.add(static_cast<SceneObjectKind>(i));
    scene.configureObject(SceneObjectKind::Obj, glm::scale(glm::mat4(1),glm::vec3(2)), glm::vec3(0), 1);
    scene.setPickingTriangles(SceneObjectKind::Obj, {{-1,-1,0},{1,-1,0},{0,1,0}});
    auto* original = scene.find(1);
    original->position = {2,0,0};
    const auto copy = scene.duplicate(1);
    check(copy > 3 && scene.find(copy), "copy receives a stable unique ID");
    check(scene.find(copy)->pickingTriangles == scene.find(1)->pickingTriangles, "copy shares geometry");
    scene.find(copy)->position.y = 3;
    check(close(scene.find(1)->position.y,0), "copy transform is independent");
    scene.update(0);
    check(scene.modelTransforms().size() == 2, "both OBJ instances rendered");
    scene.find(copy)->visible = false;
    scene.update(0);
    check(scene.modelTransforms().size() == 1, "hidden instances omitted");
    check(close(scene.find(1)->intersectRay({2,0,5},{0,0,-1}),5), "ray hits triangle in transformed instance");
    check(std::isinf(scene.find(1)->intersectRay({3.8f,1.8f,5},{0,0,-1})), "bounds alone do not select empty triangle space");
    original = scene.find(1);
    original->rotation = {20,30,40}; original->scale = {1,2,3};
    auto matrix = original->editorMatrix();
    glm::vec3 translation, rotation, scale;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), &translation.x, &rotation.x, &scale.x);
    ModelObject rebuilt;
    rebuilt.position = translation; rebuilt.rotation = rotation; rebuilt.scale = scale;
    auto recomposed = rebuilt.editorMatrix();
    for (int i=0; i<16; ++i) check(close(glm::value_ptr(matrix)[i],glm::value_ptr(recomposed)[i]), "gizmo decomposition preserves TRS");
    check(scene.remove(1) && !scene.find(1), "delete removes only its ID");
    check(scene.find(copy), "delete preserves another instance");
    scene.remove(copy);
    const auto newObj = scene.add(SceneObjectKind::Obj);
    check(close(scene.find(newObj)->importTransform[0][0],2), "asset normalization survives deletion of all instances");
    check(scene.find(newObj)->pickingTriangles != nullptr, "asset picking survives deletion of all instances");
    const auto gltfCopy = scene.duplicate(2), skinCopy = scene.duplicate(3);
    check(scene.find(gltfCopy)->kind == SceneObjectKind::Gltf && scene.find(skinCopy)->kind == SceneObjectKind::Skinned,
        "all model types duplicate correctly");
    const auto cpuCopy = scene.duplicate(4), gpuCopy = scene.duplicate(5);
    check(scene.find(cpuCopy)->kind == SceneObjectKind::CpuEmitter
        && scene.find(gpuCopy)->kind == SceneObjectKind::GpuEmitter, "particle emitters are scene objects");
    scene.find(cpuCopy)->particleSettings().rate = 123;
    scene.find(gpuCopy)->gpuSettings().capacity = 2048;
    check(close(scene.find(4)->particleSettings().rate,45), "CPU emitter settings are independent after duplication");
    check(scene.find(5)->gpuSettings().capacity == 8192, "GPU emitter settings are independent after duplication");
    auto* emitter = scene.find(cpuCopy);
    emitter->position = {3,2,1};
    emitter->rotation = {0,0,90};
    emitter->scale = {1,2,1};
    const auto worldParticle = glm::vec3(emitter->editorMatrix() * glm::vec4(0,1,0,1));
    check(close(worldParticle.x,1) && close(worldParticle.y,2) && close(worldParticle.z,1),
        "local particle positions follow emitter translation, rotation and scale");
    check(std::isfinite(emitter->intersectRay({3,2,5},{0,0,-1})), "emitter origin is selectable");
    scene.remove(cpuCopy);
    check(scene.find(4) && !scene.find(cpuCopy), "deleting one emitter preserves another emitter");
    std::cout << "Scene editor instance, picking and transform tests passed.\n";
}

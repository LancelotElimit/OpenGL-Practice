#include "EngineApplication.h"
#include "Project.h"
#include "Renderer.h"
#include "Scene.h"
#include "Window.h"
#include <cstdlib>
#include <iostream>
#include <vector>
void check(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
std::vector<unsigned char> pixels(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    std::vector<unsigned char> data(width * height * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    return data;
}
int main(int argc, char **argv) {
    check(argc == 2, "repository path required");
    const std::filesystem::path root = argv[1];
    Project project;
    check(project.open(root / "projects/Minimal/Minimal.lancelot"), "minimal project");
    EngineApplication engine;
    check(engine.initialize(project, false), "hidden GL engine context");
    auto &scene = engine.scene();
    scene.clear();
    auto &assets = engine.renderer().assets();
    const auto first =
        assets.importModel(scene, SceneObjectKind::Obj, root / "tests/fixtures/triangle.obj");
    const auto second =
        assets.importModel(scene, SceneObjectKind::Obj, root / "tests/fixtures/quad.obj");
    const auto red =
        assets.importModel(scene, SceneObjectKind::Gltf, root / "tests/fixtures/red.gltf");
    const auto green =
        assets.importModel(scene, SceneObjectKind::Gltf, root / "tests/fixtures/green.gltf");
    check(first && second && red && green && scene.objects().size() == 4,
          "four independent imports coexist");
    const auto firstRef = dynamic_cast<ModelObject &>(*scene.find(first)).model.asset;
    const auto secondRef = dynamic_cast<ModelObject &>(*scene.find(second)).model.asset;
    check(firstRef != secondRef && assets.obj(firstRef) != assets.obj(secondRef),
          "OBJ assets not category-global");
    check(assets.obj(firstRef)->model.indices().size() == 3 &&
              assets.obj(secondRef)->model.indices().size() == 6,
          "different meshes preserved");
    const auto cached = assets.resourceCount();
    const auto copy = scene.duplicate(first);
    check(assets.obj(dynamic_cast<ModelObject &>(*scene.find(copy)).model.asset) ==
                  assets.obj(firstRef) &&
              assets.resourceCount() == cached,
          "clones share cached immutable mesh");
    check(assets.bindModel(scene, first, root / "tests/fixtures/quad.obj"),
          "selected model rebind succeeds");
    check(dynamic_cast<ModelObject &>(*scene.find(first)).model.asset == secondRef &&
              dynamic_cast<ModelObject &>(*scene.find(copy)).model.asset == firstRef,
          "rebind does not replace clone's reference");
    check(assets.bindModel(scene, first, root / "tests/fixtures/triangle.obj"),
          "restore first asset");
    const auto before = Project::serializeScene(scene);
    check(!assets.importModel(scene, SceneObjectKind::Gltf, root / "tests/fixtures/missing.gltf") &&
              Project::serializeScene(scene) == before,
          "failed import does not replace scene or existing assets");
    scene.find(first)->position = {-.8f, .4f, 0};
    scene.find(second)->position = {.8f, .4f, 0};
    scene.find(red)->position = {-.8f, -.7f, 0};
    scene.find(green)->position = {.8f, -.7f, 0};
    scene.find(copy)->visible = false;
    const auto light = scene.add(SceneObjectKind::PointLight);
    scene.find(light)->position = {0, 2, 3};
    scene.add(SceneObjectKind::Environment);
    const auto emitter = scene.add(SceneObjectKind::CpuEmitter);
    scene.find(emitter)->particleSettings().rate = 100;
    const auto gpu = scene.add(SceneObjectKind::GpuEmitter);
    scene.find(gpu)->gpuSettings().capacity = 512;
    const auto smoke = scene.add(SceneObjectKind::Smoke);
    scene.find(smoke)->smokeSettings().enabled = true;
    const auto water = scene.add(SceneObjectKind::Water);
    scene.find(water)->waterSettings().visible = true;
    auto &renderer = engine.renderer();
    renderer.showGltfScene() = true;
    RenderOptions options;
    options.bloom = false;
    engine.update(scene, .05f, .05f, true);
    engine.render(scene, engine.camera(), 128, 128, options, .05f);
    const auto particles = renderer.simulations().stats().liveParticles;
    check(particles > 0 && renderer.stats().visibleInstances == 2,
          "simulation advances and both OBJ assets render");
    const auto image = pixels(renderer.viewportTexture());
    bool nonBackground = false;
    for (std::size_t i = 4; i < image.size(); i += 4)
        if (image[i] != image[0]) {
            nonBackground = true;
            break;
        }
    check(nonBackground, "rendered framebuffer contains scene detail");
    engine.render(scene, engine.camera(), 128, 128, options, .05f);
    check(image == pixels(renderer.viewportTexture()) &&
              renderer.simulations().stats().liveParticles == particles,
          "repeated rendering cannot advance simulation");
    engine.update(scene, 5, .05f, false);
    engine.render(scene, engine.camera(), 128, 128, options, .05f);
    check(image == pixels(renderer.viewportTexture()) &&
              renderer.simulations().stats().liveParticles == particles,
          "pause freezes CPU/GPU particles, water and smoke while rendering");
    scene.remove(emitter);
    scene.remove(smoke);
    engine.update(scene, 0, .05f, false);
    check(renderer.simulations().stats().liveParticles == 0,
          "deleted runtimes cleaned on synchronization");
    const auto skinA =
        assets.importModel(scene, SceneObjectKind::Skinned, root / "assets/SimpleSkin.gltf");
    const auto skinB =
        assets.importModel(scene, SceneObjectKind::Skinned, root / "assets/SimpleSkin.gltf");
    check(skinA && skinB, "multiple animated objects import");
    dynamic_cast<ModelObject &>(*scene.find(skinB)).model.playing = false;
    engine.update(scene, .05f, .1f, true);
    engine.update(scene, .05f, .15f, true);
    auto *animA = renderer.simulations().animation(skinA);
    auto *animB = renderer.simulations().animation(skinB);
    check(animA && animB && animA != animB && animA->playbackTime() > 0 &&
              animB->playbackTime() == 0,
          "per-object animation state is independent even with same source");
    renderer.showGltfModel() = true;
    const auto animationTime = animA->playbackTime();
    engine.render(scene, engine.camera(), 128, 128, options, .15f);
    check(animA->playbackTime() == animationTime, "rendering cannot advance animation");
    engine.present(128, 128);
    check(glGetError() == GL_NO_ERROR,
          "GL render, simulation and standalone presentation are error free");
    std::cout
        << "Multi-asset caching, real GL rendering, pause and simulation separation passed.\n";
}

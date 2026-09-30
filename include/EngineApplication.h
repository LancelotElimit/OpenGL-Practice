#pragma once
#include "RenderOptions.h"
#include <memory>
class Project;
class Window;
class Scene;
class Camera;
class Renderer;
// Reusable engine session. The host owns its loop and any editor UI.
class EngineApplication {
  public:
    EngineApplication();
    ~EngineApplication();
    EngineApplication(const EngineApplication &) = delete;
    EngineApplication &operator=(const EngineApplication &) = delete;
    bool initialize(Project &project, bool visible = true);
    Window &window();
    Scene &scene();
    Camera &camera();
    Renderer &renderer();
    std::size_t textureCount() const;
    void present(int width, int height);
    void update(Scene &scene, float delta, float time, bool advance);
    void render(Scene &scene, Camera &camera, int width, int height, const RenderOptions &options,
                float simulationTime);

  private:
    struct State;
    std::unique_ptr<State> state_;
};

#pragma once
#include "Scene.h"
#include "Camera.h"
#include <functional>
#include <map>

// Engine-facing API: project code supplies behaviours; the engine never knows
// what a PlayerController does. Input is sampled once per frame by the host.
struct GameInput { glm::vec2 movement{0}; };
struct ScriptContext {
    SceneObject& object;
    Camera& camera;
    GameInput input;
    float deltaTime = 0;
};
class ScriptBehaviour {
public:
    virtual ~ScriptBehaviour() = default;
    virtual void OnStart(ScriptContext&) {}
    virtual void OnUpdate(ScriptContext&) {}
    virtual void OnStop(ScriptContext&) {}
};
class ScriptRegistry {
public:
    using Factory = std::function<std::unique_ptr<ScriptBehaviour>()>;
    void add(std::string name, Factory factory) { factories_[std::move(name)]=std::move(factory); }
    std::unique_ptr<ScriptBehaviour> create(const std::string& name) const {
        auto it=factories_.find(name); return it==factories_.end()?nullptr:it->second();
    }
    const auto& entries() const { return factories_; }
private:
    std::map<std::string,Factory> factories_;
};

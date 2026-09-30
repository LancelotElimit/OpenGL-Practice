#pragma once
#include "ScriptBehaviour.h"

enum class PlayState { Editing, Playing, Paused };
enum class PlayCommand { None, Start, Pause, Resume, Stop };
class PlaySession {
public:
    explicit PlaySession(const ScriptRegistry& registry):registry_(registry) {}
    bool start(const Scene& editing, Camera& camera);
    void pause() { if(state_==PlayState::Playing) state_=PlayState::Paused; }
    void resume() { if(state_==PlayState::Paused) state_=PlayState::Playing; }
    void stop(Camera& camera);
    void update(float deltaTime, GameInput input, Camera& camera);
    Scene& activeScene(Scene& editing) { return active()?runtime_:editing; }
    PlayState state() const { return state_; }
    bool active() const { return state_!=PlayState::Editing; }
    float time() const { return time_; }
    const std::string& error() const { return error_; }
    const ScriptRegistry& registry() const { return registry_; }
    PlayCommand command = PlayCommand::None; // Applied at the next frame boundary.
private:
    struct Instance { std::uint32_t owner; std::unique_ptr<ScriptBehaviour> behaviour; };
    const ScriptRegistry& registry_;
    Scene runtime_;
    Camera editorCamera_;
    PlayState state_=PlayState::Editing;
    std::vector<Instance> scripts_;
    float time_=0;
    std::string error_;
};

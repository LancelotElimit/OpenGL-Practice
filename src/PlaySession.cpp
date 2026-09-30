#include "PlaySession.h"
#include <algorithm>
#include <cmath>

bool PlaySession::start(const Scene& editing,Camera& camera) {
    if(active()) return false;
    error_.clear();
    Scene candidate=editing;
    std::vector<Instance> instances;
    int players=0;
    for(const auto& object:candidate.objects()) {
        if(object.script.mainCharacter) {
            ++players;
            if(!object.script.enabled || object.script.type.empty()) {
                error_="The main character needs an enabled script."; return false;
            }
        }
        if(!object.script.enabled || object.script.type.empty()) continue;
        auto behaviour=registry_.create(object.script.type);
        if(!behaviour) { error_="Script is not compiled/registered: "+object.script.type; return false; }
        instances.push_back({object.id,std::move(behaviour)});
    }
    if(players>1) { error_="Only one main character can receive player input."; return false; }
    runtime_=std::move(candidate); scripts_=std::move(instances);
    editorCamera_=camera; time_=0; state_=PlayState::Playing;
    try {
        for(auto& script:scripts_) {
            ScriptContext context{*runtime_.find(script.owner),camera,{},0};
            script.behaviour->OnStart(context);
        }
    } catch(const std::exception& error) {
        error_=error.what(); stop(camera); return false;
    }
    return true;
}
void PlaySession::stop(Camera& camera) {
    if(!active()) return;
    for(auto& script:scripts_) if(auto* owner=runtime_.find(script.owner)) {
        ScriptContext context{*owner,camera,{},0};
        try { script.behaviour->OnStop(context); }
        catch(const std::exception& error) { error_=error.what(); }
    }
    scripts_.clear(); runtime_.clear(); camera=editorCamera_;
    state_=PlayState::Editing; time_=0;
}
void PlaySession::update(float deltaTime,GameInput input,Camera& camera) {
    if(state_!=PlayState::Playing) return;
    const float dt=std::isfinite(deltaTime)?std::clamp(deltaTime,0.f,.05f):0.f;
    time_+=dt;
    try {
        for(auto& script:scripts_) if(auto* owner=runtime_.find(script.owner); owner && owner->visible) {
            ScriptContext context{*owner,camera,owner->script.mainCharacter?input:GameInput{},dt};
            script.behaviour->OnUpdate(context);
        }
    } catch(const std::exception& error) { error_=error.what(); pause(); }
    runtime_.update(time_);
}

#include "PlaySession.h"
#include "ProjectScripts.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message) { if(!value) { std::cerr<<message<<'\n';std::exit(1); } }
int starts=0,updates=0,stops=0;
class Probe final:public ScriptBehaviour {
public:
    void OnStart(ScriptContext&) override { ++starts; }
    void OnUpdate(ScriptContext&) override { ++updates; }
    void OnStop(ScriptContext&) override { ++stops; }
};
class Broken final:public ScriptBehaviour {
    void OnUpdate(ScriptContext&) override { throw std::runtime_error("test script error"); }
};
int main() {
    ScriptRegistry registry; registerProjectScripts(registry);
    registry.add("Probe",[]{return std::make_unique<Probe>();});
    registry.add("Broken",[]{return std::make_unique<Broken>();});
    Scene editing;
    const auto id=editing.add(SceneObjectKind::Obj);
    auto& binding=editing.find(id)->script;
    binding.type="PlayerController"; binding.mainCharacter=true; binding.moveSpeed=4;
    const auto probe=editing.add(SceneObjectKind::CpuEmitter);
    editing.find(probe)->script.type="Probe";
    Camera camera; const auto originalCamera=camera.position();
    PlaySession play(registry);
    check(play.start(editing,camera) && starts==1,"start invokes project script lifecycle");
    check(camera.position()!=originalCamera,"follow camera activates");
    auto& running=play.activeScene(editing);
    play.update(.05f,{{1,1}},camera);
    const auto moved=running.find(id)->position;
    check(glm::length(moved)>.19f && glm::length(moved)<.201f,"diagonal movement normalized and dt applied");
    check(editing.find(id)->position==glm::vec3(0),"runtime movement cannot modify editing scene");
    const auto time=play.time(); const auto pausedCamera=camera.position();
    play.pause(); play.update(5,{{1,0}},camera);
    check(play.time()==time && running.find(id)->position==moved && updates==1
        && camera.position()==pausedCamera,"pause freezes scripts, transforms, clock and follow camera");
    play.resume(); play.update(.05f,{{1,0}},camera);
    check(updates==2 && running.find(id)->position.x>moved.x,"resume continues scripts");
    play.stop(camera);
    check(!play.active() && stops==1 && camera.position()==originalCamera,"stop restores editor camera and lifecycle");
    check(play.activeScene(editing).find(id)->position==glm::vec3(0),"stop restores original editing scene");
    check(play.start(editing,camera) && play.activeScene(editing).find(id)->position==glm::vec3(0),"replay starts from original transform");
    play.stop(camera);
    const auto clone=editing.duplicate(id);
    check(!editing.find(clone)->script.mainCharacter && editing.find(clone)->script.type=="PlayerController",
        "clone retains script without duplicating player ownership");
    editing.find(clone)->script.mainCharacter=true;
    check(!play.start(editing,camera) && !play.active(),"multiple main characters rejected without entering play");
    editing.find(clone)->script.mainCharacter=false;
    editing.find(clone)->script.type="Missing";
    check(!play.start(editing,camera),"unregistered C++ scripts produce a clear start error");
    editing.find(clone)->script.enabled=false;
    editing.find(probe)->script.type="Broken";
    check(play.start(editing,camera),"disabled missing script does not block start");
    play.update(.05f,{},camera);
    check(play.state()==PlayState::Paused && !play.error().empty(),"script exception pauses safely");
    play.stop(camera);
    std::cout<<"Play session, lifecycle, input, pause, restore and script error tests passed.\n";
}

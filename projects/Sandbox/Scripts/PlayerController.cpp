#include "ProjectScripts.h"
#include "ScriptBehaviour.h"
#include <cmath>

// This is project/game code, built separately from LancelotEngine.
// No collisions or animation state machine: movement directly edits Transform.
class PlayerController final : public ScriptBehaviour {
    void follow(ScriptContext& context) {
        if(context.object.script.mainCharacter && context.object.script.followCamera)
            context.camera.lookAt(context.object.position+context.object.script.cameraOffset,
                                  context.object.position+glm::vec3(0,.35f,0));
    }
public:
    void OnStart(ScriptContext& context) override { follow(context); }
    void OnUpdate(ScriptContext& context) override {
        glm::vec2 move=context.input.movement;
        if(glm::length(move)>1) move=glm::normalize(move);
        auto& actor=context.object;
        actor.position+=glm::vec3(move.x,0,-move.y)*actor.script.moveSpeed*context.deltaTime;
        if(actor.script.faceMovement && glm::length(move)>.001f)
            actor.rotation.y=glm::degrees(std::atan2(-move.x,move.y));
        follow(context);
    }
};
void registerProjectScripts(ScriptRegistry& registry) {
    registry.add("PlayerController",[]{return std::make_unique<PlayerController>();});
}

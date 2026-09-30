#pragma once
#include "Input.h"
#include "ScriptBehaviour.h"
#include "RenderOptions.h"
#include "PlaySession.h"
class Window;
class EditorWorkspace;
class EditorInputRouter {
public:
    bool interactive() const { return interactive_; }
    void enterEditor(Window& window,Camera& camera);
    void globalShortcuts(const InputSnapshot& input,PlaySession& play,Window& window,
                         Camera& camera,RenderOptions& options,float deltaTime);
    GameInput route(const InputSnapshot& input,PlaySession& play,EditorWorkspace& ui,
                    Camera& camera,float deltaTime);
private:
    bool interactive_=true;
};

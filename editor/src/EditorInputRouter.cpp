#include "EditorInputRouter.h"
#include "Window.h"
#include "PlaySession.h"
#include "EditorWorkspace.h"
#include <imgui.h>
#include <algorithm>
void EditorInputRouter::enterEditor(Window& window,Camera& camera) {
    interactive_=true; glfwSetInputMode(window.get(),GLFW_CURSOR,GLFW_CURSOR_NORMAL);
    camera.resetMouseSample();
}
void EditorInputRouter::globalShortcuts(const InputSnapshot& input,PlaySession& play,
    Window& window,Camera& camera,RenderOptions& options,float dt) {
    if(input.escape && !ImGui::GetIO().WantTextInput) {
        if(play.active()) play.pause();
        else glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
    }
    if(input.toggleCamera && !play.active() && !ImGui::GetIO().WantTextInput) {
        interactive_=!interactive_;
        glfwSetInputMode(window.get(),GLFW_CURSOR,interactive_?GLFW_CURSOR_NORMAL:GLFW_CURSOR_DISABLED);
        camera.resetMouseSample();
    }
    if(input.toggleBloom && !ImGui::GetIO().WantTextInput) options.bloom=!options.bloom;
    if(!play.active() && !ImGui::GetIO().WantCaptureKeyboard) {
        options.exposure=std::clamp(options.exposure+input.exposureAxis*dt,.1f,5.f);
        options.metallic=std::clamp(options.metallic+input.metallicAxis*dt*.5f,0.f,1.f);
        options.roughness=std::clamp(options.roughness+input.roughnessAxis*dt*.5f,.05f,1.f);
    }
    options.shadowPreview=input.shadowPreview; options.grayscale=input.grayscale;
}
GameInput EditorInputRouter::route(const InputSnapshot& input,PlaySession& play,
    EditorWorkspace& ui,Camera& camera,float dt) {
    GameInput game;
    if(!play.active() && input.focused && (!interactive_ || ui.sceneNavigating())) {
        camera.move(input.movement.x,input.movement.y,dt);
        camera.rotateFromPointer(input.mouseX,input.mouseY);
    } else camera.resetMouseSample();
    if(play.active() && input.focused && ui.gameInputFocused() && !input.control && !input.alt) {
        // UI event queue preserves short taps missed by a held-key snapshot.
        const auto down=[](ImGuiKey key){return ImGui::IsKeyDown(key)||ImGui::IsKeyPressed(key,false);};
        game.movement={float(down(ImGuiKey_D))-float(down(ImGuiKey_A)),
                       float(down(ImGuiKey_W))-float(down(ImGuiKey_S))};
    }
    return game;
}

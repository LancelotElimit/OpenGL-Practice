#include "EngineApplication.h"
#include "Camera.h"
#include "FrameClock.h"
#include "Input.h"
#include "Scene.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#if __has_include(<imgui.h>) || __has_include(<ImGuizmo.h>) || __has_include("EditorWorkspace.h")
#error Engine consumer must not inherit editor headers or UI dependencies
#endif
void check(bool condition,const char* message) {
    if(!condition) { std::cerr<<message<<'\n';std::exit(1); }
}
int main() {
    EngineApplication unopened; // Public engine API needs no editor/context to exist.
    FrameClock clock(10);
    check(std::abs(clock.tick(10.25)-.25f)<.0001f,"wall clock measures full frame time");
    check(FrameClock::simulationDelta(2)==.05f,"simulation time is bounded independently");
    check(clock.tick(9)==0,"clock cannot emit a negative delta");
    Camera camera;
    const auto original=camera.position();
    camera.move(1,0,.1f);
    check(std::abs(camera.position().x-original.x-.25f)<.0001f,"camera accepts abstract axes, not platform keys");
    camera.resetMouseSample();camera.rotateFromPointer(100,100);
    const auto forward=camera.front();camera.rotateFromPointer(130,100);
    check(camera.front()!=forward,"pointer sampling rotates camera without GLFW");
    Scene scene;scene.add(SceneObjectKind::Platform);
    check(scene.objects().size()==1,"scene is usable by an engine-only consumer");
    std::cout<<"Engine API, clock and input boundary tests passed without editor dependencies.\n";
}

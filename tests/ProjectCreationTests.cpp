#include "Project.h"
#include "Scene.h"
#include "EngineApplication.h"
#include "Window.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
void require(bool value, const char *message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto scratch =
        std::filesystem::temp_directory_path() / ("LancelotCreateTest-" + std::to_string(stamp));
    require(std::filesystem::create_directory(scratch), "isolated parent directory");
    // Test Unicode paths independently of the compiler/source file encoding.
    const auto encoded = std::u8string(u8"\u6d4b\u8bd5\u9879\u76ee");
    const std::string name(encoded.begin(), encoded.end());
    Project project;
    require(project.create(scratch, name), "create Unicode-named independent project");
    const auto directory = scratch / std::filesystem::u8path(name);
    require(project.descriptor() == directory / std::filesystem::u8path(name + ".lancelot"),
            "descriptor created in chosen directory");
    require(std::filesystem::is_directory(project.assetRoot()), "assets directory exists");
    require(project.name() == name, "UTF-8 project name retained");
    Scene scene;
    require(project.loadScene(scene) && scene.objects().size() == 4, "basic scene loads");
    const auto before = Project::serializeScene(scene);
    require(!project.create(scratch, name), "existing directory is never overwritten");
    require(project.loadScene(scene) && before == Project::serializeScene(scene),
            "failed creation leaves existing project intact");
    for (const char *invalid :
         {"", "..", "../outside", "CON", "nul.txt", "COM1", "bad:", "end.", "end "})
        require(!project.create(scratch, invalid), "invalid/path traversal/reserved name rejected");
    require(!project.create(scratch / "missing", "Example"), "missing parent rejected");
    require(project.saveScene(scene), "save Unicode project and create backups");
    Project reopened;
    require(reopened.open(project.descriptor()) && reopened.loadScene(scene),
            "reopen saved project");
    {
        EngineApplication engine;
        require(engine.initialize(reopened, false),
                "new external Unicode project initializes renderer");
        engine.render(engine.scene(), engine.camera(), 320, 180, RenderOptions{}, 0);
        require(glGetError() == GL_NO_ERROR, "new project's basic scene renders without GL errors");
    }
    // No broad recursive deletion: all paths below were created exclusively by this test.
    for (const auto &file : {project.descriptor(), directory / "scene.json"}) {
        std::filesystem::remove(file);
        std::filesystem::remove(std::filesystem::path(file.wstring() + L".bak"));
    }
    std::filesystem::remove(directory / "assets");
    std::filesystem::remove(directory);
    std::filesystem::remove(scratch);
    std::cout << "Project creation and Unicode persistence passed\n";
}

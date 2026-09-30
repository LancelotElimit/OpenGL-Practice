#include "Project.h"
#include "AssetPaths.h"
#include "EngineApplication.h"
#include "ScriptBehaviour.h"
#include "ProjectScripts.h"
#include <iostream>
int main(int argc,char** argv) {
    ScriptRegistry scripts;
    registerProjectScripts(scripts);
    auto path=argc>1 ? std::filesystem::path(argv[1]) : findAssetPath("projects/Sandbox/Sandbox.lancelot");
    for (;;) {
        Project project;
        if(!project.open(path)) { std::cerr << project.error() << '\n'; return 1; }
        const int result=EngineApplication{}.run(project,scripts);
        if(result || project.requestedOpen.empty()) return result;
        path=project.requestedOpen;
    }
}

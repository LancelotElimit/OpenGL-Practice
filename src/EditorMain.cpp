#include "Project.h"
#include "AssetPaths.h"
#include "EngineApplication.h"
#include <iostream>
int main(int argc,char** argv) {
    auto path=argc>1 ? std::filesystem::path(argv[1]) : findAssetPath("projects/Sandbox/Sandbox.lancelot");
    for (;;) {
        Project project;
        if(!project.open(path)) { std::cerr << project.error() << '\n'; return 1; }
        const int result=EngineApplication{}.run(project);
        if(result || project.requestedOpen.empty()) return result;
        path=project.requestedOpen;
    }
}

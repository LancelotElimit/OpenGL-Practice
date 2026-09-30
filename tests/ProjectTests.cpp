#include "Project.h"
#include "Scene.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstdlib>
void require(bool value,const char* message) { if(!value) { std::cerr<<message<<'\n';std::exit(1); } }
int main(int argc,char** argv) {
    Project sample;
    require(argc==2 && sample.open(argv[1]),"sample descriptor loads");
    Scene scene;
    require(sample.loadScene(scene),"sample scene loads");
    require(scene.objects().size()==13,"all sample contents are objects");
    require(scene.find(1)->script.mainCharacter && scene.find(1)->script.type=="PlayerController",
        "project loads main character and compiled script binding");
    require(dynamic_cast<WaterObject*>(scene.find(6)) && dynamic_cast<SmokeObject*>(scene.find(7))
        && dynamic_cast<PlatformObject*>(scene.find(8)),"concrete inherited object types retained");
    const auto copy=scene.duplicate(6);
    scene.find(copy)->waterSettings().viscosity=.8f;
    require(scene.find(6)->waterSettings().viscosity!=.8f,"water settings clone independently");
    scene.find(copy)->position={4,5,6};
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto scratch=std::filesystem::temp_directory_path()/("LancelotProjectTest-"+std::to_string(stamp));
    require(std::filesystem::create_directory(scratch),"create isolated project test folder");
    const auto descriptor=scratch/"test.lancelot", sceneFile=scratch/"scene.json";
    { std::ofstream out(descriptor); out<<R"({"version":1,"name":"Independent Project","assetRoot":"assets","scene":"scene.json","assets":{}})"; }
    { std::ofstream out(sceneFile); out<<R"({"version":1,"objects":[]})"; }
    Project independent;
    require(independent.open(descriptor),"open a project outside the engine checkout");
    require(independent.assetRoot()==scratch/"assets","asset root belongs to chosen project");
    independent.setAsset("gltf",scratch/"assets"/"replacement.glb");
    require(independent.saveScene(scene),"save scene to independent project");
    Project reopened;
    require(reopened.open(descriptor) && reopened.asset("gltf")==scratch/"assets"/"replacement.glb",
        "edited asset reference persists in the project descriptor");
    Scene restored;
    require(independent.loadScene(restored),"saved scene reloads");
    require(restored.objects().size()==14,"duplicated object persists");
    require(restored.find(1)->script.mainCharacter && restored.find(1)->script.moveSpeed==2.5f
        && restored.find(1)->script.followCamera,"script properties survive scene save/load");
    require(restored.find(copy)->waterSettings().viscosity==.8f
        && restored.find(copy)->position==glm::vec3(4,5,6),"simulation settings and transform persist");
    require(dynamic_cast<WaterObject*>(restored.find(copy))!=nullptr,"save/load preserves subclass");
    for(int i=0;i<static_cast<int>(SceneObjectKind::Count);++i) {
        const auto kind=static_cast<SceneObjectKind>(i);
        const auto id=restored.add(kind);
        const auto cloned=restored.duplicate(id);
        require(cloned && restored.find(cloned)->kind==kind,"every object type can be added and cloned");
        restored.find(cloned)->position={7,8,9};
        restored.find(cloned)->rotation={20,30,40};
        restored.find(cloned)->scale={1,2,3};
        require(restored.find(id)->position!=restored.find(cloned)->position,"cloned transforms are independent");
        require(restored.remove(cloned) && restored.find(id),"deleting each type preserves its source");
    }
    const auto before=restored.objects().size();
    { std::ofstream out(sceneFile); out<<R"({"version":1,"objects":[{"type":"Unknown"}]})"; }
    require(!independent.loadScene(restored) && restored.objects().size()==before,"invalid scene does not partially replace live scene");
    std::filesystem::remove(descriptor); std::filesystem::remove(sceneFile);
    std::filesystem::remove(sceneFile.string()+".bak");
    std::filesystem::remove(descriptor.string()+".bak"); std::filesystem::remove(scratch);
    std::cout<<"Project loading, scene persistence and polymorphic cloning passed.\n";
}

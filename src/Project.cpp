#include "Project.h"
#include "Scene.h"
#include <json.hpp>
#include <fstream>
#include <cmath>
using nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ParticleSettings, enabled, rate, lifetime, speed, gravity, startSize, endSize, preset, soft)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GpuParticleSettings, visible, emitting, paused, capacity, emissionRate, lifetime, gravity, size, preset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FluidSettings, visible, showWindow, paused, pour, pourRate, gravity, pressure, viscosity, opacity, refraction, absorption, reflectivity, foam, roughness, surfaceHz, shadingMode, viewMode)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Fluid2DSettings, showWindow, enabled, paused, autoSource, centerObstacle, sourceStrength, force, dissipation, brushRadius, pressureIterations, viewMode, opacity)
namespace {
template<class T> T settingsFrom(const json& value) {
    json merged=T{}; merged.update(value); return merged.get<T>();
}
glm::vec3 vector(const json& value) {
    if (!value.is_array() || value.size()!=3) throw std::runtime_error("A transform vector must contain three numbers");
    glm::vec3 result(value[0].get<float>(),value[1].get<float>(),value[2].get<float>());
    if (!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.z)) throw std::runtime_error("Non-finite transform");
    return result;
}
json vector(const glm::vec3& value) { return json::array({value.x,value.y,value.z}); }
json read(const std::filesystem::path& file) {
    std::ifstream input(file);
    if (!input) throw std::runtime_error("Cannot open: " + file.string());
    return json::parse(input);
}
}
bool Project::open(const std::filesystem::path& descriptor) {
    try {
        const auto absolute = std::filesystem::absolute(descriptor);
        const auto data = read(absolute);
        if (data.value("version",0)!=1) throw std::runtime_error("Unsupported project version");
        auto root = (absolute.parent_path()/data.value("assetRoot",std::string("assets"))).lexically_normal();
        auto scene = (absolute.parent_path()/data.at("scene").get<std::string>()).lexically_normal();
        const auto sceneData=read(scene);
        if (!sceneData.at("objects").is_array()) throw std::runtime_error("Scene objects must be an array");
        std::unordered_map<std::string,std::string> assets;
        for (auto it=data.at("assets").begin();it!=data.at("assets").end();++it) assets[it.key()]=it.value().get<std::string>();
        const auto radius=data.value("modelRadius",1.f);
        if (!std::isfinite(radius) || radius<=0) throw std::runtime_error("Invalid model radius");
        descriptor_=absolute; assetRoot_=root; scenePath_=scene;
        assets_=std::move(assets); name_=data.value("name",absolute.stem().string()); error_.clear();
        excludedObject_=data.value("excludeObject",std::string{});
        modelRadius_=radius;
        return true;
    } catch(const std::exception& error) { error_=error.what(); return false; }
}
std::filesystem::path Project::asset(const std::string& key) const {
    const auto entry=assets_.find(key);
    if (entry==assets_.end()||entry->second.empty()) return {};
    return (assetRoot_/entry->second).lexically_normal();
}
void Project::setAsset(const std::string& key,const std::filesystem::path& path) {
    // Keep references relative where possible; never move the user's source file.
    assets_[key]=std::filesystem::absolute(path).lexically_relative(assetRoot_).generic_string();
    if(assets_[key].empty()) assets_[key]=std::filesystem::absolute(path).generic_string();
}
bool Project::loadScene(Scene& scene) {
    try {
        const auto data=read(scenePath_);
        if (data.value("version",0)!=1) throw std::runtime_error("Unsupported scene version");
        Scene candidate=scene; candidate.clear();
        for(const auto& entry:data.at("objects")) {
            const auto type=entry.at("type").get<std::string>();
            SceneObjectKind kind=SceneObjectKind::Count;
            for(int i=0;i<static_cast<int>(SceneObjectKind::Count);++i)
                if(type==Scene::typeName(static_cast<SceneObjectKind>(i))) kind=static_cast<SceneObjectKind>(i);
            if(kind==SceneObjectKind::Count) throw std::runtime_error("Unknown object type: "+type);
            auto& object=*candidate.find(candidate.add(kind));
            object.name=entry.value("name",type); object.visible=entry.value("visible",true);
            const auto script=entry.value("script",json::object());
            object.script.type=script.value("type",std::string{});
            object.script.enabled=script.value("enabled",true);
            object.script.mainCharacter=script.value("mainCharacter",false);
            object.script.followCamera=script.value("followCamera",true);
            object.script.faceMovement=script.value("faceMovement",false);
            object.script.moveSpeed=script.value("moveSpeed",2.5f);
            object.script.cameraOffset=vector(script.value("cameraOffset",json::array({0,2,5})));
            if(!std::isfinite(object.script.moveSpeed) || object.script.moveSpeed<0 || object.script.moveSpeed>100)
                throw std::runtime_error("Script move speed must be in [0,100]");
            object.position=vector(entry.value("position",vector(object.position)));
            object.rotation=vector(entry.value("rotation",vector(object.rotation)));
            object.scale=vector(entry.value("scale",vector(object.scale)));
            if (glm::any(glm::lessThanEqual(object.scale,glm::vec3(0)))) throw std::runtime_error("Scale must be positive");
            const auto settings=entry.value("settings",json::object());
            if(kind==SceneObjectKind::CpuEmitter) object.particleSettings()=settingsFrom<ParticleSettings>(settings);
            if(kind==SceneObjectKind::GpuEmitter) object.gpuSettings()=settingsFrom<GpuParticleSettings>(settings);
            if(kind==SceneObjectKind::Water) object.waterSettings()=settingsFrom<FluidSettings>(settings);
            if(kind==SceneObjectKind::Smoke) object.smokeSettings()=settingsFrom<Fluid2DSettings>(settings);
            if(auto* light=dynamic_cast<LightObject*>(&object)) {
                light->settings.color=vector(settings.value("color",json::array({1,1,1})));
                light->settings.intensity=settings.value("intensity",3.f);
            }
            if(auto* platform=dynamic_cast<PlatformObject*>(&object)) {
                platform->tint=vector(settings.value("tint",json::array({1,1,1})));
                platform->roughness=settings.value("roughness",.78f);
            }
            if(auto* camera=dynamic_cast<CameraObject*>(&object)) camera->fov=settings.value("fov",45.f);
            if(auto* environment=dynamic_cast<EnvironmentObject*>(&object)) {
                environment->settings.sky=settings.value("sky",true);
                environment->settings.intensity=settings.value("intensity",1.f);
            }
        }
        scene=std::move(candidate); scene.update(0); error_.clear(); return true;
    } catch(const std::exception& error) { error_=error.what(); return false; }
}
bool Project::saveScene(const Scene& scene) {
    try {
        json data={{"version",1},{"objects",json::array()}};
        for(const auto& object:scene.objects()) {
            json settings=json::object();
            if(object.kind==SceneObjectKind::CpuEmitter) settings=object.particleSettings();
            if(object.kind==SceneObjectKind::GpuEmitter) settings=object.gpuSettings();
            if(object.kind==SceneObjectKind::Water) settings=object.waterSettings();
            if(object.kind==SceneObjectKind::Smoke) settings=object.smokeSettings();
            if(const auto* light=dynamic_cast<const LightObject*>(&object)) settings={{"color",vector(light->settings.color)},{"intensity",light->settings.intensity}};
            if(const auto* platform=dynamic_cast<const PlatformObject*>(&object)) settings={{"tint",vector(platform->tint)},{"roughness",platform->roughness}};
            if(const auto* camera=dynamic_cast<const CameraObject*>(&object)) settings={{"fov",camera->fov}};
            if(const auto* environment=dynamic_cast<const EnvironmentObject*>(&object)) settings={{"sky",environment->settings.sky},{"intensity",environment->settings.intensity}};
            data["objects"].push_back({{"type",Scene::typeName(object.kind)},{"name",object.name},{"visible",object.visible},
                {"position",vector(object.position)},{"rotation",vector(object.rotation)},{"scale",vector(object.scale)},{"settings",settings},
                {"script",{{"type",object.script.type},{"enabled",object.script.enabled},
                    {"mainCharacter",object.script.mainCharacter},{"followCamera",object.script.followCamera},
                    {"faceMovement",object.script.faceMovement},{"moveSpeed",object.script.moveSpeed},
                    {"cameraOffset",vector(object.script.cameraOffset)}}}});
        }
        const auto text=data.dump(2);
        std::filesystem::copy_file(scenePath_,scenePath_.string()+".bak",std::filesystem::copy_options::overwrite_existing);
        std::ofstream output(scenePath_,std::ios::trunc);
        output << text << '\n'; output.close();
        if(!output) throw std::runtime_error("Cannot save scene");
        auto manifest=read(descriptor_);
        manifest["assets"]=assets_;
        std::filesystem::copy_file(descriptor_,descriptor_.string()+".bak",std::filesystem::copy_options::overwrite_existing);
        std::ofstream descriptorOutput(descriptor_,std::ios::trunc);
        descriptorOutput<<manifest.dump(2)<<'\n'; descriptorOutput.close();
        if(!descriptorOutput) throw std::runtime_error("Cannot save project asset references");
        error_.clear(); return true;
    } catch(const std::exception& error) { error_=error.what(); return false; }
}

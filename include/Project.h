#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
class Scene;
class Project {
  public:
    bool open(const std::filesystem::path &descriptor);
    // Creates a new name/ folder below an existing parent; never overwrites a project.
    bool create(const std::filesystem::path &parent, const std::string &name);
    bool loadScene(Scene &scene);
    bool saveScene(const Scene &scene);
    static std::string serializeScene(const Scene &scene);
    static bool deserializeScene(const std::string &text, Scene &scene, std::string &error);
    std::filesystem::path asset(const std::string &key) const;
    void setAsset(const std::string &key, const std::filesystem::path &path);
    const std::filesystem::path &descriptor() const { return descriptor_; }
    const std::filesystem::path &assetRoot() const { return assetRoot_; }
    const std::string &name() const { return name_; }
    const std::string &error() const { return error_; }
    const std::string &excludedObject() const { return excludedObject_; }
    float modelRadius() const { return modelRadius_; }
    std::filesystem::path requestedOpen;

  private:
    std::filesystem::path descriptor_, assetRoot_, scenePath_;
    std::string name_, error_;
    std::string excludedObject_;
    float modelRadius_ = 1;
    std::unordered_map<std::string, std::string> assets_;
};

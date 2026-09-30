#pragma once
#include "Project.h"
#include "Scene.h"
#include <deque>
// Bounded authored-scene snapshots. No GL resources or simulation buffers copied.
class SceneHistory {
  public:
    explicit SceneHistory(std::size_t limit = 64) : limit_(limit) {}
    void reset(const Scene &scene) {
        undo_.clear();
        redo_.clear();
        baseline_ = scene;
        key_ = Project::serializeScene(scene);
    }
    bool observe(const Scene &scene) {
        auto key = Project::serializeScene(scene);
        if (key == key_)
            return false;
        undo_.push_back(baseline_);
        if (undo_.size() > limit_)
            undo_.pop_front();
        redo_.clear();
        baseline_ = scene;
        key_ = std::move(key);
        return true;
    }
    bool undo(Scene &scene) { return restore(undo_, redo_, scene); }
    bool redo(Scene &scene) { return restore(redo_, undo_, scene); }
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

  private:
    bool restore(std::deque<Scene> &from, std::deque<Scene> &to, Scene &scene) {
        if (from.empty())
            return false;
        to.push_back(scene);
        scene = std::move(from.back());
        from.pop_back();
        baseline_ = scene;
        key_ = Project::serializeScene(scene);
        scene.update(0);
        return true;
    }
    std::size_t limit_;
    Scene baseline_;
    std::string key_;
    std::deque<Scene> undo_, redo_;
};

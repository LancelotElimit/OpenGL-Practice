#pragma once

#include "ShaderProgram.h"

#include <filesystem>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class GltfAnimatedModel {
public:
    GltfAnimatedModel(
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& modelPath
    );
    ~GltfAnimatedModel();

    GltfAnimatedModel(const GltfAnimatedModel&) = delete;
    GltfAnimatedModel& operator=(const GltfAnimatedModel&) = delete;

    bool valid() const;
    void update(float timeSeconds);
    void draw(
        const glm::mat4& viewProjection,
        const glm::mat4& modelTransform,
        const glm::vec3& cameraPosition,
        const glm::vec3& lightPosition
    ) const;
    const glm::vec3& boundsCenter() const;
    float boundsRadius() const;
    void destroy();

private:
    struct NodePose {
        glm::vec3 translation{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        int parent = -1;
    };

    struct AnimationChannel {
        int node = -1;
        std::string path;
        std::string interpolation;
        std::vector<float> times;
        std::vector<glm::vec4> values;
    };

    bool load(const std::filesystem::path& modelPath);
    glm::mat4 nodeLocalMatrix(std::size_t nodeIndex) const;
    void updateGlobalTransforms();

    ShaderProgram program_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    int meshNode_ = -1;
    std::vector<NodePose> basePoses_;
    std::vector<NodePose> currentPoses_;
    std::vector<glm::mat4> globalTransforms_;
    std::vector<int> jointNodes_;
    std::vector<glm::mat4> inverseBindMatrices_;
    std::vector<glm::mat4> boneMatrices_;
    std::vector<AnimationChannel> animationChannels_;
    float animationDuration_ = 0.0f;
    glm::vec3 boundsCenter_{0.0f};
    float boundsRadius_ = 0.0f;
    bool valid_ = false;
};

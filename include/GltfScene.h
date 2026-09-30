#pragma once

#include "ShaderProgram.h"

#include <filesystem>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

// Static glTF scene path. Animated skins remain in GltfAnimatedModel.
class GltfScene {
public:
    explicit GltfScene(const std::filesystem::path& shaderDirectory);
    ~GltfScene();
    GltfScene(const GltfScene&) = delete;
    GltfScene& operator=(const GltfScene&) = delete;

    bool load(const std::filesystem::path& path);
    void draw(const glm::mat4& viewProjection, const glm::mat4& world,
              const glm::vec3& camera, const glm::vec3& light,
              bool transparent, const glm::vec3& lightColor, float ambientIntensity) const;
    void destroy();
    bool valid() const;
    const std::string& status() const;
    const glm::vec3& center() const;
    float radius() const;
    const std::vector<glm::vec3>& pickingTriangles() const { return pickingTriangles_; }
    std::size_t primitiveCount() const;
    std::size_t triangleCount(bool transparent) const;
    std::size_t drawCount(bool transparent) const;

private:
    struct Material {
        glm::vec4 baseFactor{1.0f};
        glm::vec3 emissiveFactor{0.0f};
        float metallic = 1.0f;
        float roughness = 1.0f;
        float alphaCutoff = 0.5f;
        int alphaMode = 0;
        bool doubleSided = false;
        int baseTexture = -1;
        int metalRoughTexture = -1;
        int normalTexture = -1;
        int occlusionTexture = -1;
        int emissiveTexture = -1;
    };
    struct Primitive {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
        int material = -1;
        glm::mat4 transform{1.0f};
        glm::vec3 center{0.0f};
    };

    ShaderProgram program_;
    std::vector<Primitive> primitives_;
    std::vector<glm::vec3> pickingTriangles_;
    std::vector<Material> materials_;
    std::vector<GLuint> textures_;
    glm::vec3 center_{0.0f};
    float radius_ = 0.0f;
    std::string status_ = "No glTF loaded";
    bool valid_ = false;
    void clearAsset();
};

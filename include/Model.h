#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct ModelPart {
    GLsizei firstIndex = 0;
    GLsizei indexCount = 0;
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    std::string diffuseTextureName;
};

// Loads OBJ geometry and keeps material splits, but does not own OpenGL VAOs.
class Model {
public:
    static constexpr std::size_t VertexStrideFloats = 12;

    bool load(
        const std::filesystem::path& path,
        const std::string& excludedObjectName = {}
    );

    const std::vector<float>& vertices() const;
    const std::vector<std::uint32_t>& indices() const;
    const std::vector<ModelPart>& parts() const;
    std::vector<std::string> diffuseTextureNames() const;
    const glm::vec3& boundsCenter() const;
    float boundsRadius() const;

private:
    std::vector<float> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<ModelPart> parts_;
    std::vector<std::string> diffuseTextureNames_;
    glm::vec3 boundsCenter_{0.0f};
    float boundsRadius_ = 0.0f;
};

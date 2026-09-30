#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

struct ModelPart {
    GLsizei firstIndex = 0;
    GLsizei indexCount = 0;
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    std::string diffuseTextureName;
};

struct EmbeddedImage {
    int width = 0, height = 0; // height == 0: compressed image bytes
    std::vector<unsigned char> bytes;
};
// Static geometry with material splits. File format never reaches the renderer.
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
    const std::string& status() const { return status_; }
    const std::unordered_map<std::string, EmbeddedImage>& embeddedImages() const { return embedded_; }

private:
    bool loadAssimp(const std::filesystem::path& path);
    std::string status_;
    std::unordered_map<std::string, EmbeddedImage> embedded_;
    std::vector<float> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<ModelPart> parts_;
    std::vector<std::string> diffuseTextureNames_;
    glm::vec3 boundsCenter_{0.0f};
    float boundsRadius_ = 0.0f;
};

#pragma once

#include "ShaderProgram.h"

#include <filesystem>

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

// Generates and owns the environment maps used by image-based lighting.
class EnvironmentIBL {
public:
    EnvironmentIBL(
        const std::filesystem::path& shaderDirectory,
        const std::filesystem::path& environmentPath
    );
    ~EnvironmentIBL();

    EnvironmentIBL(const EnvironmentIBL&) = delete;
    EnvironmentIBL& operator=(const EnvironmentIBL&) = delete;

    bool valid() const;
    void bind(
        GLuint irradianceUnit,
        GLuint prefilterUnit,
        GLuint brdfUnit
    ) const;
    void renderSkybox(
        const glm::mat4& view,
        const glm::mat4& projection
    ) const;
    void destroy();

private:
    void createGeometry();
    bool loadEquirectangular(
        const std::filesystem::path& environmentPath
    );
    bool generateMaps();
    void drawCube() const;
    void drawQuad() const;

    ShaderProgram environmentProgram_;
    ShaderProgram equirectangularProgram_;
    ShaderProgram irradianceProgram_;
    ShaderProgram prefilterProgram_;
    ShaderProgram brdfProgram_;
    ShaderProgram skyboxProgram_;

    GLuint environmentMap_ = 0;
    GLuint equirectangularMap_ = 0;
    GLuint irradianceMap_ = 0;
    GLuint prefilterMap_ = 0;
    GLuint brdfLut_ = 0;
    GLuint captureFramebuffer_ = 0;
    GLuint captureRenderbuffer_ = 0;
    GLuint cubeVao_ = 0;
    GLuint cubeVbo_ = 0;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    bool valid_ = false;
    bool usingExternalEnvironment_ = false;
};

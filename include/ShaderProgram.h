#pragma once

#include <glad/gl.h>

#include <filesystem>

// Owns one linked vertex/fragment shader program.
// The class hides shader compilation and uniform lookup from the renderer.
class ShaderProgram {
public:
    ShaderProgram() = default;
    ShaderProgram(
        const char* vertexSource,
        const char* fragmentSource,
        const char* debugName
    );
    ShaderProgram(
        const std::filesystem::path& vertexPath,
        const std::filesystem::path& fragmentPath,
        const char* debugName
    );

    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    bool valid() const;
    GLuint id() const;
    void use() const;
    GLint uniform(const char* name) const;
    void destroy();

private:
    void build(
        const char* vertexSource,
        const char* fragmentSource,
        const char* debugName
    );

    GLuint program_ = 0;
};

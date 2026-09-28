#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include <glm/mat4x4.hpp>

struct VertexAttribute {
    GLuint location;
    GLint componentCount;
    GLuint offsetFloats;
};

// Owns a VAO/VBO pair for one interleaved vertex layout.
class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(
        const float* data,
        std::size_t floatCount,
        GLuint strideFloats,
        std::initializer_list<VertexAttribute> attributes
    );
    void uploadIndexed(
        const float* vertexData,
        std::size_t floatCount,
        GLuint strideFloats,
        std::initializer_list<VertexAttribute> attributes,
        const std::uint32_t* indexData,
        std::size_t indexCount
    );
    void updateInstanceTransforms(
        const glm::mat4* transforms,
        std::size_t instanceCount
    );

    void bind() const;
    void draw(GLenum primitive = GL_TRIANGLES) const;
    void drawInstanced(GLenum primitive = GL_TRIANGLES) const;
    void drawRange(
        GLsizei firstIndex,
        GLsizei indexCount,
        GLenum primitive = GL_TRIANGLES
    ) const;
    void drawRangeInstanced(
        GLsizei firstIndex,
        GLsizei indexCount,
        GLenum primitive = GL_TRIANGLES
    ) const;
    GLuint vao() const;
    GLsizei vertexCount() const;
    void destroy();

private:
    void release();

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint instanceVbo_ = 0;
    GLsizei vertexCount_ = 0;
    GLsizei indexCount_ = 0;
    GLsizei instanceCount_ = 0;
};

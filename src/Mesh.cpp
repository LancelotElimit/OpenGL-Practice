#include "Mesh.h"

#include <cstddef>

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_),
      vbo_(other.vbo_),
      ebo_(other.ebo_),
      instanceVbo_(other.instanceVbo_),
      vertexCount_(other.vertexCount_),
      indexCount_(other.indexCount_),
      instanceCount_(other.instanceCount_) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.instanceVbo_ = 0;
    other.vertexCount_ = 0;
    other.indexCount_ = 0;
    other.instanceCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    release();
    vao_ = other.vao_;
    vbo_ = other.vbo_;
    ebo_ = other.ebo_;
    instanceVbo_ = other.instanceVbo_;
    vertexCount_ = other.vertexCount_;
    indexCount_ = other.indexCount_;
    instanceCount_ = other.instanceCount_;
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.instanceVbo_ = 0;
    other.vertexCount_ = 0;
    other.indexCount_ = 0;
    other.instanceCount_ = 0;
    return *this;
}

void Mesh::upload(
    const float* data,
    std::size_t floatCount,
    GLuint strideFloats,
    std::initializer_list<VertexAttribute> attributes
) {
    release();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(floatCount * sizeof(float)),
        data,
        GL_STATIC_DRAW
    );

    for (const VertexAttribute& attribute : attributes) {
        glVertexAttribPointer(
            attribute.location,
            attribute.componentCount,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(strideFloats * sizeof(float)),
            reinterpret_cast<void*>(
                static_cast<std::size_t>(attribute.offsetFloats)
                * sizeof(float)
            )
        );
        glEnableVertexAttribArray(attribute.location);
    }

    vertexCount_ = static_cast<GLsizei>(floatCount / strideFloats);
    indexCount_ = 0;
    glBindVertexArray(0);
}

void Mesh::uploadIndexed(
    const float* vertexData,
    std::size_t floatCount,
    GLuint strideFloats,
    std::initializer_list<VertexAttribute> attributes,
    const std::uint32_t* indexData,
    std::size_t indexCount
) {
    release();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(floatCount * sizeof(float)),
        vertexData,
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indexCount * sizeof(std::uint32_t)),
        indexData,
        GL_STATIC_DRAW
    );

    for (const VertexAttribute& attribute : attributes) {
        glVertexAttribPointer(
            attribute.location,
            attribute.componentCount,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(strideFloats * sizeof(float)),
            reinterpret_cast<void*>(
                static_cast<std::size_t>(attribute.offsetFloats)
                * sizeof(float)
            )
        );
        glEnableVertexAttribArray(attribute.location);
    }

    vertexCount_ = static_cast<GLsizei>(floatCount / strideFloats);
    indexCount_ = static_cast<GLsizei>(indexCount);
    glBindVertexArray(0);
}

void Mesh::bind() const {
    glBindVertexArray(vao_);
}

void Mesh::updateInstanceTransforms(
    const glm::mat4* transforms,
    std::size_t instanceCount
) {
    glBindVertexArray(vao_);
    if (instanceVbo_ == 0) {
        glGenBuffers(1, &instanceVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);

        // A mat4 occupies four consecutive vec4 vertex attributes.
        for (GLuint column = 0; column < 4; ++column) {
            const GLuint location = 4 + column;
            glVertexAttribPointer(
                location,
                4,
                GL_FLOAT,
                GL_FALSE,
                sizeof(glm::mat4),
                reinterpret_cast<void*>(
                    static_cast<std::size_t>(column)
                    * sizeof(glm::vec4)
                )
            );
            glEnableVertexAttribArray(location);
            glVertexAttribDivisor(location, 1);
        }
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    }

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(instanceCount * sizeof(glm::mat4)),
        transforms,
        GL_DYNAMIC_DRAW
    );
    instanceCount_ = static_cast<GLsizei>(instanceCount);
    glBindVertexArray(0);
}

void Mesh::draw(GLenum primitive) const {
    glBindVertexArray(vao_);
    if (ebo_ != 0) {
        glDrawElements(
            primitive,
            indexCount_,
            GL_UNSIGNED_INT,
            nullptr
        );
    } else {
        glDrawArrays(primitive, 0, vertexCount_);
    }
}

void Mesh::drawInstanced(GLenum primitive) const {
    if (instanceCount_ == 0) {
        return;
    }

    glBindVertexArray(vao_);
    if (ebo_ != 0) {
        glDrawElementsInstanced(
            primitive,
            indexCount_,
            GL_UNSIGNED_INT,
            nullptr,
            instanceCount_
        );
    } else {
        glDrawArraysInstanced(
            primitive,
            0,
            vertexCount_,
            instanceCount_
        );
    }
}

void Mesh::drawRange(
    GLsizei firstIndex,
    GLsizei indexCount,
    GLenum primitive
) const {
    glBindVertexArray(vao_);
    if (ebo_ != 0) {
        glDrawElements(
            primitive,
            indexCount,
            GL_UNSIGNED_INT,
            reinterpret_cast<void*>(
                static_cast<std::size_t>(firstIndex)
                * sizeof(std::uint32_t)
            )
        );
    } else {
        glDrawArrays(primitive, firstIndex, indexCount);
    }
}

void Mesh::drawRangeInstanced(
    GLsizei firstIndex,
    GLsizei indexCount,
    GLenum primitive
) const {
    if (instanceCount_ == 0) {
        return;
    }

    glBindVertexArray(vao_);
    if (ebo_ != 0) {
        glDrawElementsInstanced(
            primitive,
            indexCount,
            GL_UNSIGNED_INT,
            reinterpret_cast<void*>(
                static_cast<std::size_t>(firstIndex)
                * sizeof(std::uint32_t)
            ),
            instanceCount_
        );
    } else {
        glDrawArraysInstanced(
            primitive,
            firstIndex,
            indexCount,
            instanceCount_
        );
    }
}

GLuint Mesh::vao() const {
    return vao_;
}

GLsizei Mesh::vertexCount() const {
    return vertexCount_;
}

void Mesh::destroy() {
    release();
}

void Mesh::release() {
    if (instanceVbo_ != 0) {
        glDeleteBuffers(1, &instanceVbo_);
        instanceVbo_ = 0;
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertexCount_ = 0;
    indexCount_ = 0;
    instanceCount_ = 0;
}

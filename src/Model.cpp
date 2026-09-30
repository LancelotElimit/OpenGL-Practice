#include "Model.h"
#include "AssetFormats.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <cstdint>
#include <unordered_map>

namespace {

struct VertexKey {
    int positionIndex;
    int texcoordIndex;
    int normalIndex;

    bool operator==(const VertexKey&) const = default;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const noexcept {
        std::size_t hash = std::hash<int>{}(key.positionIndex);
        hash ^= std::hash<int>{}(key.texcoordIndex)
            + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= std::hash<int>{}(key.normalIndex)
            + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        return hash;
    }
};

} // namespace

bool Model::load(
    const std::filesystem::path& path,
    const std::string& excludedObjectName
) {
    embedded_.clear();
    status_.clear();
    if (AssetFormats::extension(path) != ".obj") {
        try {
            return loadAssimp(path);
        } catch (const std::exception& error) {
            status_ = error.what();
            return false;
        }
    }
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;

    const std::string baseDirectory = path.parent_path().string();
    const bool loaded = tinyobj::LoadObj(
        &attributes,
        &shapes,
        &materials,
        &warning,
        &error,
        path.string().c_str(),
        baseDirectory.c_str(),
        true
    );

    if (!warning.empty()) {
        std::cerr << "OBJ warning: " << warning << '\n';
    }
    if (!loaded) {
        status_ = error;
        std::cerr << "Could not load OBJ: " << error << '\n';
        return false;
    }

    vertices_.clear();
    indices_.clear();
    parts_.clear();
    diffuseTextureNames_.clear();
    boundsCenter_ = glm::vec3(0.0f);
    boundsRadius_ = 0.0f;

    for (const auto& material : materials) {
        if (!material.diffuse_texname.empty()) {
            diffuseTextureNames_.push_back(material.diffuse_texname);
        }
    }

    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash>
        uniqueVertices;

    for (const auto& shape : shapes) {
        if (shape.name == excludedObjectName && !excludedObjectName.empty()) {
            continue;
        }
        std::size_t indexOffset = 0;
        int previousMaterialIndex = -2;
        for (std::size_t face = 0;
             face < shape.mesh.num_face_vertices.size();
             ++face) {
            const int faceVertexCount =
                shape.mesh.num_face_vertices[face];
            const int materialIndex =
                face < shape.mesh.material_ids.size()
                    ? shape.mesh.material_ids[face]
                    : -1;

            ModelPart part;
            part.firstIndex = static_cast<GLsizei>(indices_.size());

            for (int vertex = 0; vertex < faceVertexCount; ++vertex) {
                const auto& index =
                    shape.mesh.indices[indexOffset + vertex];

                const VertexKey key{
                    index.vertex_index,
                    index.texcoord_index,
                    index.normal_index
                };
                const auto existingVertex = uniqueVertices.find(key);
                if (existingVertex != uniqueVertices.end()) {
                    indices_.push_back(existingVertex->second);
                    continue;
                }

                const auto newIndex = static_cast<std::uint32_t>(
                    vertices_.size() / VertexStrideFloats
                );
                uniqueVertices.emplace(key, newIndex);
                indices_.push_back(newIndex);

                const int positionIndex = 3 * index.vertex_index;
                vertices_.push_back(attributes.vertices[positionIndex]);
                vertices_.push_back(attributes.vertices[positionIndex + 1]);
                vertices_.push_back(attributes.vertices[positionIndex + 2]);

                if (index.texcoord_index >= 0) {
                    const int texcoordIndex = 2 * index.texcoord_index;
                    vertices_.push_back(
                        attributes.texcoords[texcoordIndex]
                    );
                    vertices_.push_back(
                        attributes.texcoords[texcoordIndex + 1]
                    );
                } else {
                    vertices_.insert(vertices_.end(), {0.0f, 0.0f});
                }

                if (index.normal_index >= 0) {
                    const int normalIndex = 3 * index.normal_index;
                    vertices_.push_back(attributes.normals[normalIndex]);
                    vertices_.push_back(
                        attributes.normals[normalIndex + 1]
                    );
                    vertices_.push_back(
                        attributes.normals[normalIndex + 2]
                    );
                } else {
                    vertices_.insert(
                        vertices_.end(),
                        {0.0f, 1.0f, 0.0f}
                    );
                }

                // Tangent xyz and handedness w are calculated after all
                // triangle indices have been collected.
                vertices_.insert(
                    vertices_.end(),
                    {0.0f, 0.0f, 0.0f, 1.0f}
                );
            }

            part.indexCount = static_cast<GLsizei>(indices_.size())
                - part.firstIndex;
            if (materialIndex >= 0
                && materialIndex < static_cast<int>(materials.size())) {
                const auto& material = materials[materialIndex];
                part.diffuseColor = glm::vec3(
                    material.diffuse[0],
                    material.diffuse[1],
                    material.diffuse[2]
                );
                part.diffuseTextureName = material.diffuse_texname;
            }
            // Consecutive faces with the same material share one draw call.
            // Exported OBJ files often contain thousands of separate faces.
            if (!parts_.empty() && materialIndex == previousMaterialIndex) {
                parts_.back().indexCount += part.indexCount;
            } else {
                parts_.push_back(part);
            }
            previousMaterialIndex = materialIndex;
            indexOffset += faceVertexCount;
        }
    }

    if (indices_.empty()) {
        std::cerr << "OBJ has no drawable faces after filtering: "
                  << path << '\n';
        return false;
    }

    const std::size_t vertexCount =
        vertices_.size() / VertexStrideFloats;
    std::vector<glm::vec3> tangentSums(vertexCount, glm::vec3(0.0f));
    std::vector<glm::vec3> bitangentSums(vertexCount, glm::vec3(0.0f));

    const auto positionAt = [this](std::uint32_t index) {
        const std::size_t start = index * VertexStrideFloats;
        return glm::vec3(
            vertices_[start],
            vertices_[start + 1],
            vertices_[start + 2]
        );
    };
    const auto texcoordAt = [this](std::uint32_t index) {
        const std::size_t start = index * VertexStrideFloats + 3;
        return glm::vec2(vertices_[start], vertices_[start + 1]);
    };

    for (std::size_t index = 0; index + 2 < indices_.size(); index += 3) {
        const std::uint32_t i0 = indices_[index];
        const std::uint32_t i1 = indices_[index + 1];
        const std::uint32_t i2 = indices_[index + 2];

        const glm::vec3 edge1 = positionAt(i1) - positionAt(i0);
        const glm::vec3 edge2 = positionAt(i2) - positionAt(i0);
        const glm::vec2 uvEdge1 = texcoordAt(i1) - texcoordAt(i0);
        const glm::vec2 uvEdge2 = texcoordAt(i2) - texcoordAt(i0);
        const float determinant =
            uvEdge1.x * uvEdge2.y - uvEdge1.y * uvEdge2.x;

        if (std::abs(determinant) < 0.000001f) {
            continue;
        }

        const float inverseDeterminant = 1.0f / determinant;
        const glm::vec3 tangent =
            (edge1 * uvEdge2.y - edge2 * uvEdge1.y)
            * inverseDeterminant;
        const glm::vec3 bitangent =
            (edge2 * uvEdge1.x - edge1 * uvEdge2.x)
            * inverseDeterminant;

        for (const std::uint32_t vertexIndex : {i0, i1, i2}) {
            tangentSums[vertexIndex] += tangent;
            bitangentSums[vertexIndex] += bitangent;
        }
    }

    for (std::size_t index = 0; index < vertexCount; ++index) {
        const std::size_t start = index * VertexStrideFloats;
        const glm::vec3 normal = glm::normalize(glm::vec3(
            vertices_[start + 5],
            vertices_[start + 6],
            vertices_[start + 7]
        ));

        glm::vec3 tangent = tangentSums[index]
            - normal * glm::dot(normal, tangentSums[index]);
        if (glm::dot(tangent, tangent) < 0.000001f) {
            const glm::vec3 reference = std::abs(normal.y) < 0.999f
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            tangent = glm::cross(reference, normal);
        }
        tangent = glm::normalize(tangent);

        const float handedness =
            glm::dot(glm::cross(normal, tangent), bitangentSums[index]) < 0.0f
                ? -1.0f
                : 1.0f;
        vertices_[start + 8] = tangent.x;
        vertices_[start + 9] = tangent.y;
        vertices_[start + 10] = tangent.z;
        vertices_[start + 11] = handedness;
    }

    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const glm::vec3 position = positionAt(
            static_cast<std::uint32_t>(index)
        );
        minimum = glm::min(minimum, position);
        maximum = glm::max(maximum, position);
    }
    boundsCenter_ = (minimum + maximum) * 0.5f;
    for (std::size_t index = 0; index < vertexCount; ++index) {
        boundsRadius_ = std::max(
            boundsRadius_,
            glm::length(
                positionAt(static_cast<std::uint32_t>(index))
                - boundsCenter_
            )
        );
    }

    return true;
}

const std::vector<float>& Model::vertices() const {
    return vertices_;
}

const std::vector<std::uint32_t>& Model::indices() const {
    return indices_;
}

const std::vector<ModelPart>& Model::parts() const {
    return parts_;
}

std::vector<std::string> Model::diffuseTextureNames() const {
    return diffuseTextureNames_;
}

const glm::vec3& Model::boundsCenter() const {
    return boundsCenter_;
}

float Model::boundsRadius() const {
    return boundsRadius_;
}

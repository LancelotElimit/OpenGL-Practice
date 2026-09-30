#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace {
glm::mat4 matrix(const aiMatrix4x4 &m) {
    return {m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4};
}
glm::vec3 vector(const aiVector3D &v) { return {v.x, v.y, v.z}; }
glm::vec3 direction(glm::vec3 value, glm::vec3 fallback) {
    const float length = glm::length(value);
    return std::isfinite(length) && length > .000001f ? value / length : fallback;
}
} // namespace
bool Model::loadAssimp(const std::filesystem::path &path) {
    Assimp::Importer importer;
    const auto *scene = importer.ReadFile(
        path.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                           aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                           aiProcess_SortByPType | aiProcess_ValidateDataStructure);
    if (!scene || !scene->mRootNode) {
        status_ = importer.GetErrorString();
        return false;
    }
    vertices_.clear();
    indices_.clear();
    parts_.clear();
    diffuseTextureNames_.clear();
    embedded_.clear();
    boundsCenter_ = {};
    boundsRadius_ = 0;
    bool bones = false;
    glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
    std::function<void(const aiNode *, glm::mat4)> visit = [&](const aiNode *node,
                                                               glm::mat4 parent) {
        const auto world = parent * matrix(node->mTransformation);
        const glm::mat3 linear(world);
        if (std::abs(glm::determinant(linear)) < .0000001f)
            return;
        const auto normalMatrix = glm::inverseTranspose(linear);
        for (unsigned n = 0; n < node->mNumMeshes; ++n) {
            const auto *mesh = scene->mMeshes[node->mMeshes[n]];
            if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
                continue;
            bones |= mesh->HasBones();
            const auto base = static_cast<std::uint32_t>(vertices_.size() / VertexStrideFloats);
            ModelPart part;
            part.firstIndex = static_cast<GLsizei>(indices_.size());
            if (mesh->mMaterialIndex < scene->mNumMaterials) {
                auto *material = scene->mMaterials[mesh->mMaterialIndex];
                aiColor4D color(1, 1, 1, 1);
                if (AI_SUCCESS != aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color))
                    aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
                part.diffuseColor = {color.r, color.g, color.b};
                aiString texturePath;
                if (AI_SUCCESS == material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) ||
                    AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath)) {
                    std::string name = texturePath.C_Str();
                    std::replace(name.begin(), name.end(), '\\', '/');
                    if (const auto *image = scene->GetEmbeddedTexture(texturePath.C_Str())) {
                        EmbeddedImage copy;
                        copy.width = static_cast<int>(image->mWidth);
                        copy.height = static_cast<int>(image->mHeight);
                        if (!image->mHeight) {
                            const auto *bytes =
                                reinterpret_cast<const unsigned char *>(image->pcData);
                            copy.bytes.assign(bytes, bytes + image->mWidth);
                        } else {
                            copy.bytes.reserve(static_cast<std::size_t>(image->mWidth) *
                                               image->mHeight * 4);
                            for (std::size_t i = 0;
                                 i < static_cast<std::size_t>(image->mWidth) * image->mHeight;
                                 ++i) {
                                const auto &pixel = image->pcData[i];
                                copy.bytes.insert(copy.bytes.end(),
                                                  {pixel.r, pixel.g, pixel.b, pixel.a});
                            }
                        }
                        embedded_.try_emplace(name, std::move(copy));
                    } else {
                        const std::filesystem::path relative(name);
                        if (!std::filesystem::exists(path.parent_path() / relative) &&
                            std::filesystem::exists(path.parent_path() / relative.filename()))
                            name = relative.filename().generic_string();
                    }
                    part.diffuseTextureName = name;
                    diffuseTextureNames_.push_back(name);
                }
            }
            for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
                const auto position = glm::vec3(world * glm::vec4(vector(mesh->mVertices[i]), 1));
                const auto normal = direction(normalMatrix * vector(mesh->mNormals[i]), {0, 1, 0});
                const glm::vec2 uv =
                    mesh->HasTextureCoords(0)
                        ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                        : glm::vec2(0);
                glm::vec3 tangent = mesh->HasTangentsAndBitangents()
                                        ? linear * vector(mesh->mTangents[i])
                                        : glm::cross(std::abs(normal.y) < .9f ? glm::vec3(0, 1, 0)
                                                                              : glm::vec3(1, 0, 0),
                                                     normal);
                tangent = direction(tangent - normal * glm::dot(normal, tangent), {1, 0, 0});
                const float handedness = mesh->HasTangentsAndBitangents() &&
                                                 glm::dot(glm::cross(normal, tangent),
                                                          linear * vector(mesh->mBitangents[i])) < 0
                                             ? -1.f
                                             : 1.f;
                vertices_.insert(vertices_.end(),
                                 {position.x, position.y, position.z, uv.x, uv.y, normal.x,
                                  normal.y, normal.z, tangent.x, tangent.y, tangent.z, handedness});
                low = glm::min(low, position);
                high = glm::max(high, position);
            }
            const bool mirrored = glm::determinant(linear) < 0;
            for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
                const auto &face = mesh->mFaces[i];
                if (face.mNumIndices != 3)
                    continue;
                indices_.insert(indices_.end(),
                                {base + face.mIndices[0], base + face.mIndices[mirrored ? 2 : 1],
                                 base + face.mIndices[mirrored ? 1 : 2]});
            }
            part.indexCount = static_cast<GLsizei>(indices_.size()) - part.firstIndex;
            if (part.indexCount)
                parts_.push_back(std::move(part));
        }
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            visit(node->mChildren[i], world);
    };
    visit(scene->mRootNode, glm::mat4(1));
    if (indices_.empty()) {
        status_ = "No triangle geometry found.";
        return false;
    }
    boundsCenter_ = (low + high) * .5f;
    for (std::size_t i = 0; i < vertices_.size(); i += VertexStrideFloats)
        boundsRadius_ = std::max(
            boundsRadius_, glm::length(glm::vec3(vertices_[i], vertices_[i + 1], vertices_[i + 2]) -
                                       boundsCenter_));
    status_ =
        bones || scene->HasAnimations()
            ? "Imported static pose; source animation is not played. Use glTF/GLB for animation."
            : "Static mesh imported.";
    return true;
}

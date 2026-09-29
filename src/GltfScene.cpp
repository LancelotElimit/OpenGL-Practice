#include "GltfScene.h"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

bool decodeImage(tinygltf::Image* image, int, std::string* error,
                 std::string*, int, int, const unsigned char* bytes,
                 int size, void*) {
    int width = 0, height = 0, components = 0;
    // Texture2D uses stb_image's global flip option; glTF needs v=0 to
    // address the image's first row, so explicitly restore the unflipped mode.
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* pixels = stbi_load_from_memory(bytes, size, &width, &height,
                                            &components, STBI_rgb_alpha);
    if (!pixels) {
        *error = "Could not decode glTF image";
        return false;
    }
    image->width = width;
    image->height = height;
    image->component = 4;
    image->bits = 8;
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    image->image.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(pixels);
    return true;
}

bool accessorBytes(const tinygltf::Model& m, int accessorIndex,
                   const unsigned char*& data, std::size_t& stride,
                   const tinygltf::Accessor*& accessor) {
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(m.accessors.size())) return false;
    accessor = &m.accessors[accessorIndex];
    if (accessor->bufferView < 0 || accessor->sparse.isSparse) return false;
    const auto& view = m.bufferViews[accessor->bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(m.buffers.size())) return false;
    const auto& buffer = m.buffers[view.buffer].data;
    const int computedStride = accessor->ByteStride(view);
    if (computedStride <= 0) return false;
    stride = static_cast<std::size_t>(computedStride);
    const std::size_t offset = view.byteOffset + accessor->byteOffset;
    if (offset > buffer.size() || accessor->count > 0
        && (accessor->count - 1) * stride + offset
            + tinygltf::GetComponentSizeInBytes(accessor->componentType)
              * tinygltf::GetNumComponentsInType(accessor->type) > buffer.size()) return false;
    data = buffer.data() + offset;
    return true;
}

glm::mat4 nodeMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        for (int i = 0; i < 16; ++i)
            glm::value_ptr(matrix)[i] = static_cast<float>(node.matrix[i]);
        return matrix;
    }
    glm::mat4 matrix(1.0f);
    if (node.translation.size() == 3)
        matrix = glm::translate(matrix, glm::vec3(node.translation[0],
            node.translation[1], node.translation[2]));
    if (node.rotation.size() == 4)
        matrix *= glm::mat4_cast(glm::quat(static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])));
    if (node.scale.size() == 3)
        matrix = glm::scale(matrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    return matrix;
}

int imageFor(const tinygltf::Model& model, int index) {
    if (index < 0 || index >= static_cast<int>(model.textures.size())) return -1;
    return model.textures[index].source;
}

} // namespace

GltfScene::GltfScene(const std::filesystem::path& shaderDirectory)
    : program_(shaderDirectory / "gltf_scene.vert",
               shaderDirectory / "gltf_scene.frag", "glTF scene shader") {}

GltfScene::~GltfScene() { destroy(); }
bool GltfScene::valid() const { return valid_; }
const std::string& GltfScene::status() const { return status_; }
const glm::vec3& GltfScene::center() const { return center_; }
float GltfScene::radius() const { return radius_; }
std::size_t GltfScene::primitiveCount() const { return primitives_.size(); }

void GltfScene::clearAsset() {
    for (const Primitive& p : primitives_) {
        glDeleteBuffers(1, &p.ebo);
        glDeleteBuffers(1, &p.vbo);
        glDeleteVertexArrays(1, &p.vao);
    }
    primitives_.clear();
    if (!textures_.empty()) glDeleteTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
    textures_.clear();
    materials_.clear();
    valid_ = false;
}

bool GltfScene::load(const std::filesystem::path& path) {
    clearAsset();
    if (!program_.valid()) { status_ = "glTF shader failed"; return false; }
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(decodeImage, nullptr);
    tinygltf::Model model;
    std::string warning, error;
    const bool parsed = path.extension() == ".glb"
        ? loader.LoadBinaryFromFile(&model, &error, &warning, path.string())
        : loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
    if (!parsed) { status_ = error.empty() ? "glTF parse failed" : error; return false; }
    if (!warning.empty()) std::cerr << "glTF warning: " << warning << '\n';

    textures_.resize(model.images.size(), 0);
    for (std::size_t i = 0; i < model.images.size(); ++i) {
        const auto& image = model.images[i];
        if (image.image.empty() || image.width <= 0 || image.height <= 0) continue;
        GLuint& texture = textures_[i];
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    materials_.reserve(model.materials.size());
    for (const auto& source : model.materials) {
        Material m;
        const auto& pbr = source.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() == 4)
            m.baseFactor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                                     pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        if (source.emissiveFactor.size() == 3)
            m.emissiveFactor = glm::vec3(source.emissiveFactor[0],
                                         source.emissiveFactor[1], source.emissiveFactor[2]);
        m.metallic = static_cast<float>(pbr.metallicFactor);
        m.roughness = static_cast<float>(pbr.roughnessFactor);
        m.alphaCutoff = static_cast<float>(source.alphaCutoff);
        m.alphaMode = source.alphaMode == "BLEND" ? 2 : source.alphaMode == "MASK" ? 1 : 0;
        m.doubleSided = source.doubleSided;
        m.baseTexture = imageFor(model, pbr.baseColorTexture.index);
        m.metalRoughTexture = imageFor(model, pbr.metallicRoughnessTexture.index);
        m.normalTexture = imageFor(model, source.normalTexture.index);
        m.occlusionTexture = imageFor(model, source.occlusionTexture.index);
        m.emissiveTexture = imageFor(model, source.emissiveTexture.index);
        materials_.push_back(m);
    }

    glm::vec3 minBound(std::numeric_limits<float>::max());
    glm::vec3 maxBound(std::numeric_limits<float>::lowest());
    std::vector<glm::vec3> allPositions;
    std::function<void(int, const glm::mat4&, int)> visit =
        [&](int nodeIndex, const glm::mat4& parent, int depth) {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()) || depth > 128) return;
        const auto& node = model.nodes[nodeIndex];
        const glm::mat4 transform = parent * nodeMatrix(node);
        if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
            for (const auto& source : model.meshes[node.mesh].primitives) {
                if (source.mode != TINYGLTF_MODE_TRIANGLES) continue;
                const auto posIt = source.attributes.find("POSITION");
                if (posIt == source.attributes.end()) continue;
                const unsigned char* posData = nullptr;
                std::size_t posStride = 0;
                const tinygltf::Accessor* posAccessor = nullptr;
                if (!accessorBytes(model, posIt->second, posData, posStride, posAccessor)
                    || posAccessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT
                    || posAccessor->type != TINYGLTF_TYPE_VEC3) continue;
                std::vector<Vertex> vertices(posAccessor->count);
                for (std::size_t i = 0; i < vertices.size(); ++i) {
                    std::memcpy(glm::value_ptr(vertices[i].position),
                                posData + i * posStride, 3 * sizeof(float));
                    const glm::vec3 world = glm::vec3(transform * glm::vec4(vertices[i].position, 1.0f));
                    minBound = glm::min(minBound, world);
                    maxBound = glm::max(maxBound, world);
                    allPositions.push_back(world);
                }
                auto readAttribute = [&](const char* name, int components, auto assign) {
                    const auto it = source.attributes.find(name);
                    if (it == source.attributes.end()) return;
                    const unsigned char* data = nullptr;
                    std::size_t stride = 0;
                    const tinygltf::Accessor* accessor = nullptr;
                    if (!accessorBytes(model, it->second, data, stride, accessor)
                        || accessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT
                        || accessor->count != vertices.size()
                        || tinygltf::GetNumComponentsInType(accessor->type) != components) return;
                    for (std::size_t i = 0; i < vertices.size(); ++i)
                        assign(vertices[i], reinterpret_cast<const float*>(data + i * stride));
                };
                readAttribute("NORMAL", 3, [](Vertex& v, const float* f) {
                    v.normal = glm::vec3(f[0], f[1], f[2]);
                });
                readAttribute("TEXCOORD_0", 2, [](Vertex& v, const float* f) {
                    v.uv = glm::vec2(f[0], f[1]);
                });
                std::vector<std::uint32_t> indices;
                if (source.indices >= 0) {
                    const unsigned char* data = nullptr;
                    std::size_t stride = 0;
                    const tinygltf::Accessor* accessor = nullptr;
                    if (!accessorBytes(model, source.indices, data, stride, accessor)) continue;
                    indices.resize(accessor->count);
                    for (std::size_t i = 0; i < indices.size(); ++i) {
                        const unsigned char* at = data + i * stride;
                        switch (accessor->componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indices[i] = *at; break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                            std::uint16_t n; std::memcpy(&n, at, sizeof(n)); indices[i] = n; break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            std::memcpy(&indices[i], at, sizeof(std::uint32_t)); break;
                        default: indices.clear(); break;
                        }
                        if (indices.empty()) break;
                    }
                } else {
                    indices.resize(vertices.size());
                    for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<std::uint32_t>(i);
                }
                if (indices.empty() || indices.size() % 3 != 0
                    || std::any_of(indices.begin(), indices.end(), [&](std::uint32_t i) { return i >= vertices.size(); })) continue;
                Primitive p;
                p.transform = transform;
                p.material = source.material;
                p.indexCount = static_cast<GLsizei>(indices.size());
                p.center = glm::vec3(transform * glm::vec4(vertices[indices[0]].position, 1.0f));
                glGenVertexArrays(1, &p.vao);
                glGenBuffers(1, &p.vbo);
                glGenBuffers(1, &p.ebo);
                glBindVertexArray(p.vao);
                glBindBuffer(GL_ARRAY_BUFFER, p.vbo);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                    reinterpret_cast<void*>(offsetof(Vertex, normal)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                    reinterpret_cast<void*>(offsetof(Vertex, uv)));
                primitives_.push_back(p);
            }
        }
        for (int child : node.children) visit(child, transform, depth + 1);
    };
    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size())) {
        for (int root : model.scenes[sceneIndex].nodes) visit(root, glm::mat4(1.0f), 0);
    }
    glBindVertexArray(0);
    if (primitives_.empty()) {
        status_ = "No supported static triangle primitives";
        clearAsset();
        return false;
    }
    center_ = (minBound + maxBound) * 0.5f;
    radius_ = 0.0f;
    for (const glm::vec3& point : allPositions)
        radius_ = std::max(radius_, glm::length(point - center_));
    status_ = "Loaded " + std::to_string(primitives_.size()) + " primitives / "
        + std::to_string(materials_.size()) + " materials";
    valid_ = true;
    return true;
}

std::size_t GltfScene::triangleCount(bool transparent) const {
    std::size_t total = 0;
    for (const Primitive& p : primitives_) {
        const int alphaMode = p.material >= 0 && p.material < static_cast<int>(materials_.size())
            ? materials_[p.material].alphaMode : 0;
        if ((alphaMode == 2) == transparent) total += p.indexCount / 3;
    }
    return total;
}
std::size_t GltfScene::drawCount(bool transparent) const {
    std::size_t total = 0;
    for (const Primitive& p : primitives_) {
        const int alphaMode = p.material >= 0 && p.material < static_cast<int>(materials_.size())
            ? materials_[p.material].alphaMode : 0;
        if ((alphaMode == 2) == transparent) ++total;
    }
    return total;
}

void GltfScene::draw(const glm::mat4& viewProjection, const glm::mat4& world,
                     const glm::vec3& camera, const glm::vec3& light,
                     bool transparent) const {
    if (!valid_) return;
    program_.use();
    const char* samplers[] = {"uBase", "uMetalRough", "uNormal", "uOcclusion", "uEmissive"};
    for (int i = 0; i < 5; ++i) glUniform1i(program_.uniform(samplers[i]), i);
    glUniformMatrix4fv(program_.uniform("uViewProjection"), 1, GL_FALSE,
                       glm::value_ptr(viewProjection));
    glUniform3fv(program_.uniform("uCamera"), 1, glm::value_ptr(camera));
    glUniform3fv(program_.uniform("uLight"), 1, glm::value_ptr(light));
    const Material fallback;
    std::vector<const Primitive*> toDraw;
    for (const Primitive& p : primitives_) {
        const Material& m = p.material >= 0 && p.material < static_cast<int>(materials_.size())
            ? materials_[p.material] : fallback;
        if ((m.alphaMode == 2) == transparent) toDraw.push_back(&p);
    }
    if (transparent) {
        std::sort(toDraw.begin(), toDraw.end(), [&](const Primitive* a, const Primitive* b) {
            const glm::vec3 da = glm::vec3(world * glm::vec4(a->center, 1.0f)) - camera;
            const glm::vec3 db = glm::vec3(world * glm::vec4(b->center, 1.0f)) - camera;
            return glm::dot(da, da) > glm::dot(db, db);
        });
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    for (const Primitive* p : toDraw) {
        const Material& m = p->material >= 0 && p->material < static_cast<int>(materials_.size())
            ? materials_[p->material] : fallback;
        const glm::mat4 transform = world * p->transform;
        glUniformMatrix4fv(program_.uniform("uModel"), 1, GL_FALSE,
                           glm::value_ptr(transform));
        glUniform4fv(program_.uniform("uBaseFactor"), 1, glm::value_ptr(m.baseFactor));
        glUniform3fv(program_.uniform("uEmissiveFactor"), 1, glm::value_ptr(m.emissiveFactor));
        glUniform1f(program_.uniform("uMetallic"), m.metallic);
        glUniform1f(program_.uniform("uRoughness"), m.roughness);
        glUniform1f(program_.uniform("uAlphaCutoff"), m.alphaCutoff);
        glUniform1i(program_.uniform("uAlphaMode"), m.alphaMode);
        const int indices[] = {m.baseTexture, m.metalRoughTexture, m.normalTexture,
                               m.occlusionTexture, m.emissiveTexture};
        const char* flags[] = {"uHasBase", "uHasMetalRough", "uHasNormal",
                               "uHasOcclusion", "uHasEmissive"};
        for (int i = 0; i < 5; ++i) {
            // Material references were resolved to image indices at load time.
            GLuint id = 0;
            if (indices[i] >= 0) {
                if (indices[i] < static_cast<int>(textures_.size())) id = textures_[indices[i]];
            }
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, id);
            glUniform1i(program_.uniform(flags[i]), id != 0);
        }
        if (m.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
        glBindVertexArray(p->vao);
        glDrawElements(GL_TRIANGLES, p->indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    if (transparent) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); }
}

void GltfScene::destroy() {
    clearAsset();
    program_.destroy();
}

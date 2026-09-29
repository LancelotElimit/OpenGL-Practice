#define GLM_ENABLE_EXPERIMENTAL
#include "GltfAnimatedModel.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace {

struct SkinnedVertex {
    glm::vec3 position{0.0f};
    glm::uvec4 joints{0u};
    glm::vec4 weights{0.0f};
};

const unsigned char* accessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor
) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

std::size_t accessorStride(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor
) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const int stride = accessor.ByteStride(view);
    return stride > 0
        ? static_cast<std::size_t>(stride)
        : tinygltf::GetComponentSizeInBytes(accessor.componentType)
            * tinygltf::GetNumComponentsInType(accessor.type);
}

template<typename T>
T readScalar(const unsigned char* data) {
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

std::uint32_t readIndex(
    const unsigned char* data,
    int componentType
) {
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return readScalar<std::uint8_t>(data);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return readScalar<std::uint16_t>(data);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return readScalar<std::uint32_t>(data);
    default:
        return 0;
    }
}

glm::uvec4 readJoints(const unsigned char* data, int componentType) {
    glm::uvec4 result(0u);
    const std::size_t componentSize =
        tinygltf::GetComponentSizeInBytes(componentType);
    for (int component = 0; component < 4; ++component) {
        result[component] = readIndex(
            data + component * componentSize,
            componentType
        );
    }
    return result;
}

glm::mat4 matrixFromGltf(const std::vector<double>& values) {
    glm::mat4 result(1.0f);
    if (values.size() == 16) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                result[column][row] = static_cast<float>(
                    values[column * 4 + row]
                );
            }
        }
    }
    return result;
}

} // namespace

GltfAnimatedModel::GltfAnimatedModel(
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& modelPath
) : program_(
        shaderDirectory / "skinned.vert",
        shaderDirectory / "skinned.frag",
        "Skinned glTF shader"
    ) {
    valid_ = program_.valid() && load(modelPath);
}

GltfAnimatedModel::~GltfAnimatedModel() {
    destroy();
}

bool GltfAnimatedModel::valid() const {
    return valid_;
}

bool GltfAnimatedModel::load(const std::filesystem::path& modelPath) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warning;
    std::string error;
    if (!loader.LoadASCIIFromFile(
            &model,
            &error,
            &warning,
            modelPath.string()
        )) {
        std::cerr << "Could not load glTF: " << error << '\n';
        return false;
    }
    if (!warning.empty()) {
        std::cerr << "glTF warning: " << warning << '\n';
    }

    basePoses_.resize(model.nodes.size());
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        const auto& node = model.nodes[index];
        NodePose pose;
        if (node.translation.size() == 3) {
            pose.translation = glm::vec3(
                node.translation[0],
                node.translation[1],
                node.translation[2]
            );
        }
        if (node.rotation.size() == 4) {
            pose.rotation = glm::quat(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2])
            );
        }
        if (node.scale.size() == 3) {
            pose.scale = glm::vec3(
                node.scale[0], node.scale[1], node.scale[2]
            );
        }
        if (node.matrix.size() == 16) {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(
                matrixFromGltf(node.matrix),
                pose.scale,
                pose.rotation,
                pose.translation,
                skew,
                perspective
            );
        }
        basePoses_[index] = pose;
        if (node.mesh >= 0 && node.skin >= 0) {
            meshNode_ = static_cast<int>(index);
        }
    }
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        for (const int child : model.nodes[index].children) {
            basePoses_[static_cast<std::size_t>(child)].parent =
                static_cast<int>(index);
        }
    }
    currentPoses_ = basePoses_;
    globalTransforms_.resize(model.nodes.size(), glm::mat4(1.0f));

    if (meshNode_ < 0) {
        std::cerr << "glTF does not contain a skinned mesh node.\n";
        return false;
    }
    const auto& meshNode = model.nodes[meshNode_];
    const auto& primitive = model.meshes[meshNode.mesh].primitives.front();
    const auto positionIt = primitive.attributes.find("POSITION");
    const auto jointsIt = primitive.attributes.find("JOINTS_0");
    const auto weightsIt = primitive.attributes.find("WEIGHTS_0");
    if (positionIt == primitive.attributes.end()
        || jointsIt == primitive.attributes.end()
        || weightsIt == primitive.attributes.end()) {
        std::cerr << "glTF skinning attributes are incomplete.\n";
        return false;
    }

    const auto& positionAccessor = model.accessors[positionIt->second];
    const auto& jointsAccessor = model.accessors[jointsIt->second];
    const auto& weightsAccessor = model.accessors[weightsIt->second];
    std::vector<SkinnedVertex> vertices(positionAccessor.count);
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const float* position = reinterpret_cast<const float*>(
            accessorData(model, positionAccessor)
                + index * accessorStride(model, positionAccessor)
        );
        vertices[index].position = glm::vec3(
            position[0], position[1], position[2]
        );
        vertices[index].joints = readJoints(
            accessorData(model, jointsAccessor)
                + index * accessorStride(model, jointsAccessor),
            jointsAccessor.componentType
        );
        const float* weights = reinterpret_cast<const float*>(
            accessorData(model, weightsAccessor)
                + index * accessorStride(model, weightsAccessor)
        );
        vertices[index].weights = glm::vec4(
            weights[0], weights[1], weights[2], weights[3]
        );
        minimum = glm::min(minimum, vertices[index].position);
        maximum = glm::max(maximum, vertices[index].position);
    }
    boundsCenter_ = (minimum + maximum) * 0.5f;
    for (const SkinnedVertex& vertex : vertices) {
        boundsRadius_ = std::max(
            boundsRadius_,
            glm::length(vertex.position - boundsCenter_)
        );
    }

    const auto& indexAccessor = model.accessors[primitive.indices];
    std::vector<std::uint32_t> indices(indexAccessor.count);
    for (std::size_t index = 0; index < indices.size(); ++index) {
        indices[index] = readIndex(
            accessorData(model, indexAccessor)
                + index * accessorStride(model, indexAccessor),
            indexAccessor.componentType
        );
    }

    const auto& skin = model.skins[meshNode.skin];
    jointNodes_ = skin.joints;
    inverseBindMatrices_.resize(jointNodes_.size(), glm::mat4(1.0f));
    if (skin.inverseBindMatrices >= 0) {
        const auto& inverseAccessor =
            model.accessors[skin.inverseBindMatrices];
        for (std::size_t index = 0; index < jointNodes_.size(); ++index) {
            const float* values = reinterpret_cast<const float*>(
                accessorData(model, inverseAccessor)
                    + index * accessorStride(model, inverseAccessor)
            );
            std::memcpy(
                glm::value_ptr(inverseBindMatrices_[index]),
                values,
                sizeof(glm::mat4)
            );
        }
    }
    boneMatrices_.resize(jointNodes_.size(), glm::mat4(1.0f));

    for (std::size_t animationIndex = 0; animationIndex < model.animations.size(); ++animationIndex) {
        const auto& animation = model.animations[animationIndex];
        AnimationClip clip;
        clip.name = animation.name.empty()
            ? "Clip " + std::to_string(animationIndex + 1) : animation.name;
        for (const auto& channel : animation.channels) {
            const auto& sampler = animation.samplers[channel.sampler];
            AnimationChannel result;
            result.node = channel.target_node;
            result.path = channel.target_path;
            result.interpolation = sampler.interpolation;

            const auto& inputAccessor = model.accessors[sampler.input];
            result.times.resize(inputAccessor.count);
            for (std::size_t index = 0;
                 index < inputAccessor.count;
                 ++index) {
                result.times[index] = readScalar<float>(
                    accessorData(model, inputAccessor)
                        + index * accessorStride(model, inputAccessor)
                );
                clip.duration = std::max(
                    clip.duration, result.times[index]
                );
            }

            const auto& outputAccessor = model.accessors[sampler.output];
            const int componentCount =
                tinygltf::GetNumComponentsInType(outputAccessor.type);
            result.values.resize(outputAccessor.count, glm::vec4(0.0f));
            for (std::size_t index = 0;
                 index < outputAccessor.count;
                 ++index) {
                const float* values = reinterpret_cast<const float*>(
                    accessorData(model, outputAccessor)
                        + index * accessorStride(model, outputAccessor)
                );
                for (int component = 0;
                     component < componentCount;
                     ++component) {
                    result.values[index][component] = values[component];
                }
            }
            clip.channels.push_back(std::move(result));
        }
        clips_.push_back(std::move(clip));
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
        vertices.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, position))
    );
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(
        1, 4, GL_UNSIGNED_INT, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, joints))
    );
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
        reinterpret_cast<void*>(offsetof(SkinnedVertex, weights))
    );
    glBindVertexArray(0);
    indexCount_ = static_cast<GLsizei>(indices.size());

    update(0.0f);
    std::cout << "glTF skin loaded: " << vertices.size()
              << " vertices, " << jointNodes_.size()
              << " joints, " << clips_.size()
              << " animation clips\n";
    return true;
}

void GltfAnimatedModel::update(float timeSeconds) {
    const float dt = lastUpdateTime_ < 0.0f ? 0.0f
        : glm::clamp(timeSeconds - lastUpdateTime_, 0.0f, 0.1f);
    lastUpdateTime_ = timeSeconds;
    if (playing_) playbackTime_ += dt * playbackSpeed_;
    currentPoses_ = basePoses_;
    const AnimationClip* clip = clips_.empty() ? nullptr : &clips_[clipIndex_];
    const float animationTime = clip && clip->duration > 0.0f
        ? std::fmod(playbackTime_, clip->duration)
        : 0.0f;
    if (clip) for (const AnimationChannel& channel : clip->channels) {
        if (channel.times.empty() || channel.node < 0) {
            continue;
        }
        const auto upper = std::upper_bound(
            channel.times.begin(),
            channel.times.end(),
            animationTime
        );
        const std::size_t next = upper == channel.times.end()
            ? channel.times.size() - 1
            : static_cast<std::size_t>(upper - channel.times.begin());
        const std::size_t previous = next == 0 ? 0 : next - 1;
        const float interval = channel.times[next] - channel.times[previous];
        float factor = interval > 0.0f
            ? (animationTime - channel.times[previous]) / interval
            : 0.0f;
        if (channel.interpolation == "STEP") {
            factor = 0.0f;
        }
        factor = glm::clamp(factor, 0.0f, 1.0f);

        const bool cubic = channel.interpolation == "CUBICSPLINE"
            && channel.values.size() == channel.times.size() * 3;
        auto sample = [&](std::size_t key) -> glm::vec4 {
            return channel.values[cubic ? key * 3 + 1 : key];
        };
        glm::vec4 interpolated;
        if (cubic && next != previous) {
            const float t = factor, t2 = t * t, t3 = t2 * t;
            const glm::vec4 p0 = sample(previous), p1 = sample(next);
            const glm::vec4 m0 = channel.values[previous * 3 + 2] * interval;
            const glm::vec4 m1 = channel.values[next * 3] * interval;
            interpolated = (2*t3 - 3*t2 + 1)*p0 + (t3 - 2*t2 + t)*m0
                + (-2*t3 + 3*t2)*p1 + (t3 - t2)*m1;
        } else {
            interpolated = glm::mix(sample(previous), sample(next), factor);
        }

        NodePose& pose = currentPoses_[channel.node];
        if (channel.path == "rotation") {
            if (cubic) {
                pose.rotation = glm::normalize(glm::quat(
                    interpolated.w, interpolated.x, interpolated.y, interpolated.z));
            } else {
                const glm::vec4 a = sample(previous), b = sample(next);
                pose.rotation = glm::normalize(glm::slerp(
                    glm::quat(a.w, a.x, a.y, a.z),
                    glm::quat(b.w, b.x, b.y, b.z), factor));
            }
        } else {
            const glm::vec4 value = interpolated;
            if (channel.path == "translation") {
                pose.translation = glm::vec3(value);
            } else if (channel.path == "scale") {
                pose.scale = glm::vec3(value);
            }
        }
    }
    if (blendRemaining_ > 0.0f && previousClipPose_.size() == currentPoses_.size()) {
        blendRemaining_ = std::max(0.0f, blendRemaining_ - dt);
        const float weight = 1.0f - blendRemaining_ / 0.3f;
        for (std::size_t i = 0; i < currentPoses_.size(); ++i) {
            currentPoses_[i].translation = glm::mix(previousClipPose_[i].translation,
                                                     currentPoses_[i].translation, weight);
            currentPoses_[i].scale = glm::mix(previousClipPose_[i].scale,
                                               currentPoses_[i].scale, weight);
            currentPoses_[i].rotation = glm::normalize(glm::slerp(
                previousClipPose_[i].rotation, currentPoses_[i].rotation, weight));
        }
    }
    updateGlobalTransforms();
}

std::size_t GltfAnimatedModel::animationCount() const { return clips_.size(); }
const std::string& GltfAnimatedModel::animationName(std::size_t index) const {
    static const std::string empty;
    return index < clips_.size() ? clips_[index].name : empty;
}
int GltfAnimatedModel::animationIndex() const { return clipIndex_; }
void GltfAnimatedModel::setAnimationIndex(int index) {
    if (index < 0 || index >= static_cast<int>(clips_.size()) || index == clipIndex_) return;
    previousClipPose_ = currentPoses_;
    clipIndex_ = index;
    playbackTime_ = 0.0f;
    blendRemaining_ = 0.3f;
}
bool& GltfAnimatedModel::playing() { return playing_; }
float& GltfAnimatedModel::playbackSpeed() { return playbackSpeed_; }

glm::mat4 GltfAnimatedModel::nodeLocalMatrix(
    std::size_t nodeIndex
) const {
    const NodePose& pose = currentPoses_[nodeIndex];
    return glm::translate(glm::mat4(1.0f), pose.translation)
        * glm::mat4_cast(pose.rotation)
        * glm::scale(glm::mat4(1.0f), pose.scale);
}

void GltfAnimatedModel::updateGlobalTransforms() {
    std::vector<bool> completed(currentPoses_.size(), false);
    std::function<void(std::size_t)> resolve = [&](std::size_t index) {
        if (completed[index]) {
            return;
        }
        const int parent = currentPoses_[index].parent;
        if (parent >= 0) {
            resolve(static_cast<std::size_t>(parent));
            globalTransforms_[index] = globalTransforms_[parent]
                * nodeLocalMatrix(index);
        } else {
            globalTransforms_[index] = nodeLocalMatrix(index);
        }
        completed[index] = true;
    };
    for (std::size_t index = 0; index < currentPoses_.size(); ++index) {
        resolve(index);
    }

    const glm::mat4 inverseMesh = glm::inverse(
        globalTransforms_[static_cast<std::size_t>(meshNode_)]
    );
    for (std::size_t index = 0; index < jointNodes_.size(); ++index) {
        boneMatrices_[index] = inverseMesh
            * globalTransforms_[jointNodes_[index]]
            * inverseBindMatrices_[index];
    }
}

void GltfAnimatedModel::draw(
    const glm::mat4& viewProjection,
    const glm::mat4& modelTransform,
    const glm::vec3& cameraPosition,
    const glm::vec3& lightPosition
) const {
    if (!valid_) {
        return;
    }
    program_.use();
    glUniformMatrix4fv(
        program_.uniform("uViewProjection"),
        1, GL_FALSE, glm::value_ptr(viewProjection)
    );
    glUniformMatrix4fv(
        program_.uniform("uModel"),
        1, GL_FALSE, glm::value_ptr(modelTransform)
    );
    glUniformMatrix4fv(
        program_.uniform("uBones[0]"),
        static_cast<GLsizei>(boneMatrices_.size()),
        GL_FALSE,
        glm::value_ptr(boneMatrices_.front())
    );
    glUniform3fv(
        program_.uniform("uCameraPosition"),
        1, glm::value_ptr(cameraPosition)
    );
    glUniform3fv(
        program_.uniform("uLightPosition"),
        1, glm::value_ptr(lightPosition)
    );
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

const glm::vec3& GltfAnimatedModel::boundsCenter() const {
    return boundsCenter_;
}

float GltfAnimatedModel::boundsRadius() const {
    return boundsRadius_;
}

std::size_t GltfAnimatedModel::triangleCount() const {
    return static_cast<std::size_t>(indexCount_) / 3;
}

void GltfAnimatedModel::destroy() {
    valid_ = false;
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    ebo_ = 0;
    vbo_ = 0;
    vao_ = 0;
    indexCount_ = 0;
    program_.destroy();
}

#include "EnvironmentIBL.h"

#include <array>
#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace {

constexpr GLsizei environmentSize = 256;
constexpr GLsizei irradianceSize = 32;
constexpr GLsizei prefilterSize = 128;
constexpr GLsizei brdfSize = 256;
constexpr int prefilterMipCount = 5;

const glm::mat4 captureProjection = glm::perspective(
    glm::radians(90.0f),
    1.0f,
    0.1f,
    10.0f
);

const std::array<glm::mat4, 6> captureViews = {{
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f)
    ),
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f)
    ),
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    ),
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f)
    ),
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, -1.0f, 0.0f)
    ),
    glm::lookAt(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, -1.0f, 0.0f)
    )
}};

void allocateCubemap(GLuint texture, GLsizei size) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (int face = 0; face < 6; ++face) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_RGB16F,
            size,
            size,
            0,
            GL_RGB,
            GL_FLOAT,
            nullptr
        );
    }
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_WRAP_R,
        GL_CLAMP_TO_EDGE
    );
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

} // namespace

EnvironmentIBL::EnvironmentIBL(
    const std::filesystem::path& shaderDirectory,
    const std::filesystem::path& environmentPath
) : environmentProgram_(
        shaderDirectory / "cubemap_capture.vert",
        shaderDirectory / "procedural_environment.frag",
        "Procedural environment shader"
    ),
    equirectangularProgram_(
        shaderDirectory / "cubemap_capture.vert",
        shaderDirectory / "equirectangular_to_cubemap.frag",
        "Equirectangular environment shader"
    ),
    irradianceProgram_(
        shaderDirectory / "cubemap_capture.vert",
        shaderDirectory / "irradiance_convolution.frag",
        "Irradiance convolution shader"
    ),
    prefilterProgram_(
        shaderDirectory / "cubemap_capture.vert",
        shaderDirectory / "prefilter_environment.frag",
        "Environment prefilter shader"
    ),
    brdfProgram_(
        shaderDirectory / "brdf_lut.vert",
        shaderDirectory / "brdf_lut.frag",
        "BRDF integration shader"
    ),
    skyboxProgram_(
        shaderDirectory / "skybox.vert",
        shaderDirectory / "skybox.frag",
        "Skybox shader"
    ) {
    if (!environmentProgram_.valid()
        || !equirectangularProgram_.valid()
        || !irradianceProgram_.valid()
        || !prefilterProgram_.valid()
        || !brdfProgram_.valid()
        || !skyboxProgram_.valid()) {
        return;
    }

    createGeometry();
    usingExternalEnvironment_ = loadEquirectangular(environmentPath);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    if (!generateMaps()) {
        return;
    }

    skyboxProgram_.use();
    glUniform1i(skyboxProgram_.uniform("uEnvironmentMap"), 0);
    valid_ = true;
    std::cout << "IBL resources generated from "
              << (usingExternalEnvironment_ ? "HDR file" : "fallback sky")
              << ": environment, irradiance, "
              << "prefilter, BRDF LUT\n";
}

EnvironmentIBL::~EnvironmentIBL() {
    destroy();
}

bool EnvironmentIBL::valid() const {
    return valid_;
}

void EnvironmentIBL::createGeometry() {
    const float cubeVertices[] = {
        -1, -1, -1,   -1, -1,  1,   -1,  1,  1,
         1,  1, -1,   -1, -1, -1,   -1,  1, -1,
         1, -1,  1,   -1, -1, -1,    1, -1, -1,
         1,  1, -1,    1, -1, -1,   -1, -1, -1,
        -1, -1, -1,   -1,  1,  1,   -1,  1, -1,
         1, -1,  1,   -1, -1,  1,   -1, -1, -1,
        -1,  1,  1,   -1, -1,  1,    1, -1,  1,
         1,  1,  1,    1, -1, -1,    1,  1, -1,
         1, -1, -1,    1,  1,  1,    1, -1,  1,
         1,  1,  1,    1,  1, -1,   -1,  1, -1,
         1,  1,  1,   -1,  1, -1,   -1,  1,  1,
         1,  1,  1,   -1,  1,  1,    1, -1,  1
    };
    glGenVertexArrays(1, &cubeVao_);
    glGenBuffers(1, &cubeVbo_);
    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(cubeVertices),
        cubeVertices,
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        nullptr
    );

    const float quadVertices[] = {
        -1, -1,
         1, -1,
        -1,  1,
         1,  1
    };
    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(quadVertices),
        quadVertices,
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(float),
        nullptr
    );
    glBindVertexArray(0);
}

bool EnvironmentIBL::loadEquirectangular(
    const std::filesystem::path& environmentPath
) {
    if (environmentPath.empty()) {
        return false;
    }

    stbi_set_flip_vertically_on_load(1);
    int width = 0;
    int height = 0;
    int channels = 0;
    float* pixels = stbi_loadf(
        environmentPath.string().c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb
    );
    if (pixels == nullptr) {
        std::cerr << "Could not load HDR environment: "
                  << environmentPath << '\n';
        return false;
    }

    glGenTextures(1, &equirectangularMap_);
    glBindTexture(GL_TEXTURE_2D, equirectangularMap_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB16F,
        width,
        height,
        0,
        GL_RGB,
        GL_FLOAT,
        pixels
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(pixels);
    return true;
}

bool EnvironmentIBL::generateMaps() {
    glGenFramebuffers(1, &captureFramebuffer_);
    glGenRenderbuffers(1, &captureRenderbuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFramebuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRenderbuffer_);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        captureRenderbuffer_
    );

    glGenTextures(1, &environmentMap_);
    allocateCubemap(environmentMap_, environmentSize);
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR
    );
    glBindRenderbuffer(GL_RENDERBUFFER, captureRenderbuffer_);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        environmentSize,
        environmentSize
    );
    const ShaderProgram& captureProgram = usingExternalEnvironment_
        ? equirectangularProgram_
        : environmentProgram_;
    captureProgram.use();
    if (usingExternalEnvironment_) {
        glUniform1i(captureProgram.uniform("uEquirectangularMap"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, equirectangularMap_);
    }
    glUniformMatrix4fv(
        captureProgram.uniform("uProjection"),
        1,
        GL_FALSE,
        glm::value_ptr(captureProjection)
    );
    glViewport(0, 0, environmentSize, environmentSize);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(
            captureProgram.uniform("uView"),
            1,
            GL_FALSE,
            glm::value_ptr(captureViews[face])
        );
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            environmentMap_,
            0
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap_);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glDeleteTextures(1, &equirectangularMap_);
    equirectangularMap_ = 0;

    glGenTextures(1, &irradianceMap_);
    allocateCubemap(irradianceMap_, irradianceSize);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRenderbuffer_);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        irradianceSize,
        irradianceSize
    );
    irradianceProgram_.use();
    glUniform1i(irradianceProgram_.uniform("uEnvironmentMap"), 0);
    glUniformMatrix4fv(
        irradianceProgram_.uniform("uProjection"),
        1,
        GL_FALSE,
        glm::value_ptr(captureProjection)
    );
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap_);
    glViewport(0, 0, irradianceSize, irradianceSize);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(
            irradianceProgram_.uniform("uView"),
            1,
            GL_FALSE,
            glm::value_ptr(captureViews[face])
        );
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            irradianceMap_,
            0
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCube();
    }

    glGenTextures(1, &prefilterMap_);
    allocateCubemap(prefilterMap_, prefilterSize);
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR
    );
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    prefilterProgram_.use();
    glUniform1i(prefilterProgram_.uniform("uEnvironmentMap"), 0);
    glUniformMatrix4fv(
        prefilterProgram_.uniform("uProjection"),
        1,
        GL_FALSE,
        glm::value_ptr(captureProjection)
    );
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap_);
    for (int mip = 0; mip < prefilterMipCount; ++mip) {
        const GLsizei mipSize = static_cast<GLsizei>(
            prefilterSize * std::pow(0.5, mip)
        );
        glBindRenderbuffer(GL_RENDERBUFFER, captureRenderbuffer_);
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH_COMPONENT24,
            mipSize,
            mipSize
        );
        glViewport(0, 0, mipSize, mipSize);
        glUniform1f(
            prefilterProgram_.uniform("uRoughness"),
            static_cast<float>(mip)
                / static_cast<float>(prefilterMipCount - 1)
        );
        for (int face = 0; face < 6; ++face) {
            glUniformMatrix4fv(
                prefilterProgram_.uniform("uView"),
                1,
                GL_FALSE,
                glm::value_ptr(captureViews[face])
            );
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                prefilterMap_,
                mip
            );
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawCube();
        }
    }

    glGenTextures(1, &brdfLut_);
    glBindTexture(GL_TEXTURE_2D, brdfLut_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RG16F,
        brdfSize,
        brdfSize,
        0,
        GL_RG,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRenderbuffer_);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        brdfSize,
        brdfSize
    );
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        brdfLut_,
        0
    );
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "IBL capture framebuffer is not complete.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    glViewport(0, 0, brdfSize, brdfSize);
    brdfProgram_.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void EnvironmentIBL::bind(
    GLuint irradianceUnit,
    GLuint prefilterUnit,
    GLuint brdfUnit
) const {
    glActiveTexture(GL_TEXTURE0 + irradianceUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap_);
    glActiveTexture(GL_TEXTURE0 + prefilterUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap_);
    glActiveTexture(GL_TEXTURE0 + brdfUnit);
    glBindTexture(GL_TEXTURE_2D, brdfLut_);
}

void EnvironmentIBL::renderSkybox(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::mat3& sampleRotation,
    float intensity
) const {
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    skyboxProgram_.use();
    glUniformMatrix3fv(skyboxProgram_.uniform("uSampleRotation"),1,GL_FALSE,glm::value_ptr(sampleRotation));
    glUniform1f(skyboxProgram_.uniform("uIntensity"),intensity);
    glUniformMatrix4fv(
        skyboxProgram_.uniform("uView"),
        1,
        GL_FALSE,
        glm::value_ptr(view)
    );
    glUniformMatrix4fv(
        skyboxProgram_.uniform("uProjection"),
        1,
        GL_FALSE,
        glm::value_ptr(projection)
    );
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap_);
    drawCube();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void EnvironmentIBL::drawCube() const {
    glBindVertexArray(cubeVao_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void EnvironmentIBL::drawQuad() const {
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void EnvironmentIBL::destroy() {
    valid_ = false;
    glDeleteTextures(1, &environmentMap_);
    glDeleteTextures(1, &equirectangularMap_);
    glDeleteTextures(1, &irradianceMap_);
    glDeleteTextures(1, &prefilterMap_);
    glDeleteTextures(1, &brdfLut_);
    glDeleteFramebuffers(1, &captureFramebuffer_);
    glDeleteRenderbuffers(1, &captureRenderbuffer_);
    glDeleteVertexArrays(1, &cubeVao_);
    glDeleteBuffers(1, &cubeVbo_);
    glDeleteVertexArrays(1, &quadVao_);
    glDeleteBuffers(1, &quadVbo_);
    environmentMap_ = 0;
    equirectangularMap_ = 0;
    irradianceMap_ = 0;
    prefilterMap_ = 0;
    brdfLut_ = 0;
    captureFramebuffer_ = 0;
    captureRenderbuffer_ = 0;
    cubeVao_ = 0;
    cubeVbo_ = 0;
    quadVao_ = 0;
    quadVbo_ = 0;
    environmentProgram_.destroy();
    equirectangularProgram_.destroy();
    irradianceProgram_.destroy();
    prefilterProgram_.destroy();
    brdfProgram_.destroy();
    skyboxProgram_.destroy();
}

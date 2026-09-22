#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// Print a useful message when a GLSL shader fails to compile.
bool checkShaderCompilation(GLuint shader, const char* shaderName) {
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success == GL_TRUE) {
        return true;
    }

    char infoLog[1024]{};
    glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
    std::cerr << shaderName << " compilation failed:\n" << infoLog << '\n';
    return false;
}

// Print a useful message when the shaders fail to link into a program.
bool checkProgramLinking(GLuint program) {
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success == GL_TRUE) {
        return true;
    }

    char infoLog[1024]{};
    glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
    std::cerr << "Shader program linking failed:\n" << infoLog << '\n';
    return false;
}

struct ModelPart {
    GLsizei firstVertex = 0;
    GLsizei vertexCount = 0;
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    std::string diffuseTextureName;
};

struct SceneNode {
    glm::mat4 localTransform = glm::mat4(1.0f);
    int parentIndex = -1;
};

int main() {
    // 1. Initialize GLFW.
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    // Request an OpenGL 3.3 core profile context.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Create the window and its OpenGL context.
    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "OpenGL Practice",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create the GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    // 3. Load modern OpenGL functions through GLAD.
    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to load OpenGL functions through GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL version: "
              << GLAD_VERSION_MAJOR(version)
              << '.'
              << GLAD_VERSION_MINOR(version)
              << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    // Enable depth testing for future 3D geometry.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // 4. Define the vertex and fragment shaders.
    const char* vertexShaderSource = R"GLSL(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 uTransform;
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

out vec2 texCoord;
out vec3 worldNormal;
out vec3 worldPosition;
out vec4 lightSpacePosition;

void main() {
    vec4 worldVertex = uModel * vec4(aPos, 1.0);
    gl_Position = uTransform * vec4(aPos, 1.0);
    texCoord = aTexCoord;
    worldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    worldPosition = vec3(worldVertex);
    lightSpacePosition = uLightSpaceMatrix * worldVertex;
}
)GLSL";

    const char* fragmentShaderSource = R"GLSL(
#version 330 core

in vec2 texCoord;
in vec3 worldNormal;
in vec3 worldPosition;
in vec4 lightSpacePosition;
uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform sampler2D uShadowMap;
uniform samplerCube uPointShadowMap;
uniform vec3 uMaterialColor;
uniform vec3 uLightPosition;
uniform vec3 uLightPosition2;
uniform vec3 uLightColor;
uniform vec3 uLightColor2;
uniform vec3 uSpotLightPosition;
uniform vec3 uSpotLightDirection;
uniform vec3 uSpotLightColor;
uniform float uSpotInnerCutoff;
uniform float uSpotOuterCutoff;
uniform float uPointShadowFarPlane;
uniform vec3 uCameraPosition;
uniform float uSpecularStrength;
uniform float uShininess;
out vec4 FragColor;

float calculateShadow(vec3 normal) {
    vec3 projectedCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    projectedCoords = projectedCoords * 0.5 + 0.5;

    if (projectedCoords.z > 1.0
        || projectedCoords.x < 0.0
        || projectedCoords.x > 1.0
        || projectedCoords.y < 0.0
        || projectedCoords.y > 1.0) {
        return 0.0;
    }

    float bias = max(
        0.002 * (1.0 - dot(normal, normalize(-uSpotLightDirection))),
        0.0005
    );
    float currentDepth = projectedCoords.z;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float shadow = 0.0;

    // Percentage-closer filtering softens the shadow edge.
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(
                uShadowMap,
                projectedCoords.xy + vec2(x, y) * texelSize
            ).r;
            shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

vec3 calculateNormalFromMap() {
    vec3 positionDerivativeX = dFdx(worldPosition);
    vec3 positionDerivativeY = dFdy(worldPosition);
    vec2 uvDerivativeX = dFdx(texCoord);
    vec2 uvDerivativeY = dFdy(texCoord);

    vec3 tangent = normalize(
        positionDerivativeX * uvDerivativeY.y
        - positionDerivativeY * uvDerivativeX.y
    );
    vec3 bitangent = normalize(
        -positionDerivativeX * uvDerivativeY.x
        + positionDerivativeY * uvDerivativeX.x
    );
    vec3 geometricNormal = normalize(worldNormal);
    mat3 tangentToWorld = mat3(tangent, bitangent, geometricNormal);

    vec3 tangentNormal = texture(uNormalMap, texCoord * 4.0).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;
    return normalize(tangentToWorld * tangentNormal);
}

float calculatePointShadow() {
    vec3 fragmentToLight = worldPosition - uLightPosition;
    float currentDepth = length(fragmentToLight);
    float closestDepth = texture(
        uPointShadowMap,
        fragmentToLight
    ).r;

    // Convert the perspective depth value back to a distance.
    float nearPlane = 0.1;
    float depthNdc = closestDepth * 2.0 - 1.0;
    closestDepth = (2.0 * nearPlane * uPointShadowFarPlane)
        / (uPointShadowFarPlane + nearPlane
            - depthNdc * (uPointShadowFarPlane - nearPlane));

    const float bias = 0.05;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

vec3 calculatePointLight(
    vec3 lightPosition,
    vec3 lightColor,
    vec3 normal,
    vec3 albedo,
    vec3 directionToCamera
) {
    vec3 directionToLight = normalize(lightPosition - worldPosition);
    float distanceToLight = length(lightPosition - worldPosition);
    float attenuation = 1.0 / (
        1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight
    );
    float diffuse = max(dot(normal, directionToLight), 0.0);
    vec3 reflectedLight = reflect(-directionToLight, normal);
    float specular = pow(
        max(dot(directionToCamera, reflectedLight), 0.0),
        uShininess
    );

    return attenuation * (
        diffuse * albedo * lightColor
        + uSpecularStrength * specular * lightColor
    );
}

vec3 calculateSpotLight(
    vec3 lightPosition,
    vec3 lightDirection,
    vec3 lightColor,
    vec3 normal,
    vec3 albedo,
    vec3 directionToCamera
) {
    vec3 directionToLight = normalize(lightPosition - worldPosition);
    float theta = dot(directionToLight, normalize(-lightDirection));
    float epsilon = uSpotInnerCutoff - uSpotOuterCutoff;
    float coneIntensity = clamp(
        (theta - uSpotOuterCutoff) / epsilon,
        0.0,
        1.0
    );

    return coneIntensity * calculatePointLight(
        lightPosition,
        lightColor,
        normal,
        albedo,
        directionToCamera
    );
}

void main() {
    vec3 normal = calculateNormalFromMap();
    vec3 directionToCamera = normalize(uCameraPosition - worldPosition);
    vec3 albedo = texture(uTexture, texCoord).rgb * uMaterialColor;
    vec3 lighting = 0.15 * albedo;

    float pointShadow = calculatePointShadow();
    lighting += (1.0 - pointShadow) * calculatePointLight(
        uLightPosition,
        uLightColor,
        normal,
        albedo,
        directionToCamera
    );
    lighting += calculatePointLight(
        uLightPosition2,
        uLightColor2,
        normal,
        albedo,
        directionToCamera
    );
    float spotlightShadow = calculateShadow(normal);
    lighting += (1.0 - spotlightShadow) * calculateSpotLight(
        uSpotLightPosition,
        uSpotLightDirection,
        uSpotLightColor,
        normal,
        albedo,
        directionToCamera
    );

    FragColor = vec4(lighting, 1.0);
}
)GLSL";

    const char* lightVertexShaderSource = R"GLSL(
#version 330 core

uniform mat4 uViewProjection;
uniform vec3 uLightPosition;

void main() {
    gl_Position = uViewProjection * vec4(uLightPosition, 1.0);
    gl_PointSize = 18.0;
}
)GLSL";

    const char* lightFragmentShaderSource = R"GLSL(
#version 330 core

uniform vec3 uMarkerColor;
out vec4 FragColor;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    if (dot(centered, centered) > 1.0) {
        discard;
    }

    FragColor = vec4(uMarkerColor, 1.0);
}
)GLSL";

    const char* depthVertexShaderSource = R"GLSL(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

void main() {
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
)GLSL";

    const char* depthFragmentShaderSource = R"GLSL(
#version 330 core

void main() {
}
)GLSL";

    const char* debugVertexShaderSource = R"GLSL(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 texCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    texCoord = aTexCoord;
}
)GLSL";

    const char* debugFragmentShaderSource = R"GLSL(
#version 330 core

in vec2 texCoord;
uniform sampler2D uDepthMap;
out vec4 FragColor;

void main() {
    float depth = texture(uDepthMap, texCoord).r;
    FragColor = vec4(vec3(depth), 1.0);
}
)GLSL";

    // 5. Compile both shaders.
    const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    if (!checkShaderCompilation(vertexShader, "Vertex Shader")) {
        glDeleteShader(vertexShader);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    if (!checkShaderCompilation(fragmentShader, "Fragment Shader")) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 6. Link the shaders into one GPU program.
    const GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!checkProgramLinking(shaderProgram)) {
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Compile a tiny second shader program used only to visualize the light.
    const GLuint lightVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(
        lightVertexShader,
        1,
        &lightVertexShaderSource,
        nullptr
    );
    glCompileShader(lightVertexShader);

    if (!checkShaderCompilation(lightVertexShader, "Light Vertex Shader")) {
        glDeleteShader(lightVertexShader);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint lightFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(
        lightFragmentShader,
        1,
        &lightFragmentShaderSource,
        nullptr
    );
    glCompileShader(lightFragmentShader);

    if (!checkShaderCompilation(lightFragmentShader, "Light Fragment Shader")) {
        glDeleteShader(lightVertexShader);
        glDeleteShader(lightFragmentShader);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint lightProgram = glCreateProgram();
    glAttachShader(lightProgram, lightVertexShader);
    glAttachShader(lightProgram, lightFragmentShader);
    glLinkProgram(lightProgram);

    glDeleteShader(lightVertexShader);
    glDeleteShader(lightFragmentShader);

    if (!checkProgramLinking(lightProgram)) {
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Compile the depth-only program used for the shadow map pass.
    const GLuint depthVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(
        depthVertexShader,
        1,
        &depthVertexShaderSource,
        nullptr
    );
    glCompileShader(depthVertexShader);

    if (!checkShaderCompilation(depthVertexShader, "Depth Vertex Shader")) {
        glDeleteShader(depthVertexShader);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint depthFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(
        depthFragmentShader,
        1,
        &depthFragmentShaderSource,
        nullptr
    );
    glCompileShader(depthFragmentShader);

    if (!checkShaderCompilation(depthFragmentShader, "Depth Fragment Shader")) {
        glDeleteShader(depthVertexShader);
        glDeleteShader(depthFragmentShader);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint depthProgram = glCreateProgram();
    glAttachShader(depthProgram, depthVertexShader);
    glAttachShader(depthProgram, depthFragmentShader);
    glLinkProgram(depthProgram);

    glDeleteShader(depthVertexShader);
    glDeleteShader(depthFragmentShader);

    if (!checkProgramLinking(depthProgram)) {
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Compile the simple fullscreen program used to inspect the shadow map.
    const GLuint debugVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(
        debugVertexShader,
        1,
        &debugVertexShaderSource,
        nullptr
    );
    glCompileShader(debugVertexShader);

    if (!checkShaderCompilation(debugVertexShader, "Debug Vertex Shader")) {
        glDeleteShader(debugVertexShader);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint debugFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(
        debugFragmentShader,
        1,
        &debugFragmentShaderSource,
        nullptr
    );
    glCompileShader(debugFragmentShader);

    if (!checkShaderCompilation(debugFragmentShader, "Debug Fragment Shader")) {
        glDeleteShader(debugVertexShader);
        glDeleteShader(debugFragmentShader);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint debugProgram = glCreateProgram();
    glAttachShader(debugProgram, debugVertexShader);
    glAttachShader(debugProgram, debugFragmentShader);
    glLinkProgram(debugProgram);

    glDeleteShader(debugVertexShader);
    glDeleteShader(debugFragmentShader);

    if (!checkProgramLinking(debugProgram)) {
        glDeleteProgram(debugProgram);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLint transformLocation =
        glGetUniformLocation(shaderProgram, "uTransform");
    const GLint modelLocation =
        glGetUniformLocation(shaderProgram, "uModel");
    const GLint textureLocation =
        glGetUniformLocation(shaderProgram, "uTexture");
    const GLint normalMapLocation =
        glGetUniformLocation(shaderProgram, "uNormalMap");
    const GLint materialColorLocation =
        glGetUniformLocation(shaderProgram, "uMaterialColor");
    const GLint lightPositionLocation =
        glGetUniformLocation(shaderProgram, "uLightPosition");
    const GLint lightPosition2Location =
        glGetUniformLocation(shaderProgram, "uLightPosition2");
    const GLint lightColorLocation =
        glGetUniformLocation(shaderProgram, "uLightColor");
    const GLint lightColor2Location =
        glGetUniformLocation(shaderProgram, "uLightColor2");
    const GLint spotLightPositionLocation =
        glGetUniformLocation(shaderProgram, "uSpotLightPosition");
    const GLint spotLightDirectionLocation =
        glGetUniformLocation(shaderProgram, "uSpotLightDirection");
    const GLint spotLightColorLocation =
        glGetUniformLocation(shaderProgram, "uSpotLightColor");
    const GLint spotInnerCutoffLocation =
        glGetUniformLocation(shaderProgram, "uSpotInnerCutoff");
    const GLint spotOuterCutoffLocation =
        glGetUniformLocation(shaderProgram, "uSpotOuterCutoff");
    const GLint cameraPositionLocation =
        glGetUniformLocation(shaderProgram, "uCameraPosition");
    const GLint specularStrengthLocation =
        glGetUniformLocation(shaderProgram, "uSpecularStrength");
    const GLint shininessLocation =
        glGetUniformLocation(shaderProgram, "uShininess");
    const GLint lightSpaceMatrixLocation =
        glGetUniformLocation(shaderProgram, "uLightSpaceMatrix");
    const GLint shadowMapLocation =
        glGetUniformLocation(shaderProgram, "uShadowMap");
    const GLint pointShadowMapLocation =
        glGetUniformLocation(shaderProgram, "uPointShadowMap");
    const GLint pointShadowFarPlaneLocation =
        glGetUniformLocation(shaderProgram, "uPointShadowFarPlane");
    const GLint lightTransformLocation =
        glGetUniformLocation(lightProgram, "uViewProjection");
    const GLint markerLightPositionLocation =
        glGetUniformLocation(lightProgram, "uLightPosition");
    const GLint markerColorLocation =
        glGetUniformLocation(lightProgram, "uMarkerColor");
    const GLint depthLightSpaceLocation =
        glGetUniformLocation(depthProgram, "uLightSpaceMatrix");
    const GLint depthModelLocation =
        glGetUniformLocation(depthProgram, "uModel");
    const GLint debugDepthMapLocation =
        glGetUniformLocation(debugProgram, "uDepthMap");

    // Each vertex contains position (x, y, z), UV (u, v), and a normal.
    // Six faces * two triangles * three vertices = 36 vertices.
    const float vertices[] = {
        // Front face: normal (0, 0, -1)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,

        // Back face: normal (0, 0, 1)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, 0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f,  1.0f,

        // Left face: normal (-1, 0, 0)
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,

        // Right face: normal (1, 0, 0)
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,

        // Bottom face: normal (0, -1, 0)
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,

        // Top face: normal (0, 1, 0)
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f, 0.0f
    };

    // Load the mesh from an OBJ file and flatten it into position/UV/normal data.
    const std::filesystem::path modelCandidates[] = {
        "assets/cube.obj",
        "../assets/cube.obj",
        "../../assets/cube.obj"
    };
    std::filesystem::path modelPath;
    for (const auto& candidate : modelCandidates) {
        if (std::filesystem::exists(candidate)) {
            modelPath = candidate;
            break;
        }
    }

    if (modelPath.empty()) {
        std::cerr << "Could not find assets/cube.obj.\n";
        glDeleteProgram(debugProgram);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    tinyobj::attrib_t modelAttributes;
    std::vector<tinyobj::shape_t> modelShapes;
    std::vector<tinyobj::material_t> modelMaterials;
    std::string modelWarning;
    std::string modelError;
    const std::string modelBaseDirectory = modelPath.parent_path().string();
    const bool modelLoaded = tinyobj::LoadObj(
        &modelAttributes,
        &modelShapes,
        &modelMaterials,
        &modelWarning,
        &modelError,
        modelPath.string().c_str(),
        modelBaseDirectory.c_str(),
        true
    );

    if (!modelWarning.empty()) {
        std::cerr << "OBJ warning: " << modelWarning << '\n';
    }
    if (!modelLoaded) {
        std::cerr << "Could not load OBJ: " << modelError << '\n';
        glDeleteProgram(debugProgram);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::vector<float> modelVertices;
    std::vector<ModelPart> modelParts;
    for (const auto& shape : modelShapes) {
        size_t indexOffset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            const int faceVertexCount = shape.mesh.num_face_vertices[face];
            const int materialIndex = face < shape.mesh.material_ids.size()
                ? shape.mesh.material_ids[face]
                : -1;
            ModelPart part;
            part.firstVertex = static_cast<GLsizei>(modelVertices.size() / 8);

            for (int vertex = 0; vertex < faceVertexCount; ++vertex) {
                const auto& index = shape.mesh.indices[indexOffset + vertex];
            const int positionIndex = 3 * index.vertex_index;
            modelVertices.push_back(modelAttributes.vertices[positionIndex]);
            modelVertices.push_back(modelAttributes.vertices[positionIndex + 1]);
            modelVertices.push_back(modelAttributes.vertices[positionIndex + 2]);

            if (index.texcoord_index >= 0) {
                const int texcoordIndex = 2 * index.texcoord_index;
                modelVertices.push_back(
                    modelAttributes.texcoords[texcoordIndex]
                );
                modelVertices.push_back(
                    modelAttributes.texcoords[texcoordIndex + 1]
                );
            } else {
                modelVertices.push_back(0.0f);
                modelVertices.push_back(0.0f);
            }

            if (index.normal_index >= 0) {
                const int normalIndex = 3 * index.normal_index;
                modelVertices.push_back(
                    modelAttributes.normals[normalIndex]
                );
                modelVertices.push_back(
                    modelAttributes.normals[normalIndex + 1]
                );
                modelVertices.push_back(
                    modelAttributes.normals[normalIndex + 2]
                );
            } else {
                modelVertices.push_back(0.0f);
                modelVertices.push_back(1.0f);
                modelVertices.push_back(0.0f);
            }
            }

            part.vertexCount = static_cast<GLsizei>(
                modelVertices.size() / 8
            ) - part.firstVertex;
            if (materialIndex >= 0
                && materialIndex < static_cast<int>(modelMaterials.size())) {
                const auto& material = modelMaterials[materialIndex];
                part.diffuseColor = glm::vec3(
                    material.diffuse[0],
                    material.diffuse[1],
                    material.diffuse[2]
                );
                part.diffuseTextureName = material.diffuse_texname;
            }
            modelParts.push_back(part);
            indexOffset += faceVertexCount;
        }
    }

    const GLsizei modelVertexCount = static_cast<GLsizei>(
        modelVertices.size() / 8
    );

    // 7. Upload the vertex data and describe its layout.
    GLuint vao = 0;
    GLuint vbo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        modelVertices.size() * sizeof(float),
        modelVertices.data(),
        GL_STATIC_DRAW
    );

    // Attribute 0: position, the first three floats of each vertex.
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        nullptr
    );
    glEnableVertexAttribArray(0);

    // Attribute 1: texture coordinates, after the position data.
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    // Attribute 2: surface normal, after position and UV data.
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float))
    );
    glEnableVertexAttribArray(2);

    // A large floor gives future shadows a surface to land on.
    const float floorVertices[] = {
        // position                UV              normal
        -5.0f, -0.5f, -5.0f,       0.0f, 0.0f,     0.0f, 1.0f, 0.0f,
         5.0f, -0.5f, -5.0f,       5.0f, 0.0f,     0.0f, 1.0f, 0.0f,
         5.0f, -0.5f,  5.0f,       5.0f, 5.0f,     0.0f, 1.0f, 0.0f,
         5.0f, -0.5f,  5.0f,       5.0f, 5.0f,     0.0f, 1.0f, 0.0f,
        -5.0f, -0.5f,  5.0f,       0.0f, 5.0f,     0.0f, 1.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,       0.0f, 0.0f,     0.0f, 1.0f, 0.0f
    };

    GLuint floorVao = 0;
    GLuint floorVbo = 0;
    glGenVertexArrays(1, &floorVao);
    glGenBuffers(1, &floorVbo);

    glBindVertexArray(floorVao);
    glBindBuffer(GL_ARRAY_BUFFER, floorVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(floorVertices),
        floorVertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        nullptr
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float))
    );
    glEnableVertexAttribArray(2);

    // A fullscreen quad for the shadow-map debug view.
    const float debugQuadVertices[] = {
        // position   UV
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f
    };

    GLuint debugVao = 0;
    GLuint debugVbo = 0;
    glGenVertexArrays(1, &debugVao);
    glGenBuffers(1, &debugVbo);
    glBindVertexArray(debugVao);
    glBindBuffer(GL_ARRAY_BUFFER, debugVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(debugQuadVertices),
        debugQuadVertices,
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        nullptr
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    // 8. Load the base color texture from an external image file.
    const std::filesystem::path textureCandidates[] = {
        "assets/basi6a08.png",
        "../assets/basi6a08.png",
        "../../assets/basi6a08.png"
    };
    std::filesystem::path texturePath;
    for (const auto& candidate : textureCandidates) {
        if (std::filesystem::exists(candidate)) {
            texturePath = candidate;
            break;
        }
    }

    if (texturePath.empty()) {
        std::cerr << "Could not find assets/basi6a08.png.\n";
        glDeleteProgram(debugProgram);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    int textureChannels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* loadedTexturePixels = stbi_load(
        texturePath.string().c_str(),
        &textureWidth,
        &textureHeight,
        &textureChannels,
        STBI_rgb_alpha
    );

    if (!loadedTexturePixels) {
        std::cerr << "Could not load texture: "
                  << texturePath.string() << '\n';
        glDeleteProgram(debugProgram);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        textureWidth,
        textureHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        loadedTexturePixels
    );
    stbi_image_free(loadedTexturePixels);

    // Load each unique diffuse texture referenced by the MTL file.
    std::unordered_map<std::string, GLuint> materialTextures;
    for (const auto& material : modelMaterials) {
        const std::string& textureName = material.diffuse_texname;
        if (textureName.empty()
            || materialTextures.find(textureName) != materialTextures.end()) {
            continue;
        }

        const std::filesystem::path materialTexturePath =
            modelPath.parent_path() / textureName;
        int materialTextureWidth = 0;
        int materialTextureHeight = 0;
        int materialTextureChannels = 0;
        unsigned char* materialPixels = stbi_load(
            materialTexturePath.string().c_str(),
            &materialTextureWidth,
            &materialTextureHeight,
            &materialTextureChannels,
            STBI_rgb_alpha
        );

        if (!materialPixels) {
            std::cerr << "Could not load material texture: "
                      << materialTexturePath.string() << '\n';
            continue;
        }

        GLuint materialTexture = 0;
        glGenTextures(1, &materialTexture);
        glBindTexture(GL_TEXTURE_2D, materialTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            materialTextureWidth,
            materialTextureHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            materialPixels
        );
        stbi_image_free(materialPixels);
        materialTextures.emplace(textureName, materialTexture);
    }

    // A tiny tangent-space normal map: RGB (128, 128, 255) is flat.
    const unsigned char normalPixels[] = {
        128, 128, 255, 255,    205, 128, 220, 255,
        128, 205, 220, 255,     75,  75, 205, 255
    };

    GLuint normalTexture = 0;
    glGenTextures(1, &normalTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        2,
        2,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        normalPixels
    );

    // Create a depth texture and framebuffer for the shadow map pass.
    constexpr GLsizei shadowMapWidth = 1024;
    constexpr GLsizei shadowMapHeight = 1024;
    GLuint shadowFramebuffer = 0;
    GLuint shadowTexture = 0;

    glGenFramebuffers(1, &shadowFramebuffer);
    glGenTextures(1, &shadowTexture);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT,
        shadowMapWidth,
        shadowMapHeight,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float shadowBorderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(
        GL_TEXTURE_2D,
        GL_TEXTURE_BORDER_COLOR,
        shadowBorderColor
    );

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        shadowTexture,
        0
    );
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer is not complete.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &shadowFramebuffer);
        glDeleteTextures(1, &shadowTexture);
        glDeleteTextures(1, &texture);
        glDeleteProgram(depthProgram);
        glDeleteProgram(lightProgram);
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create a depth cubemap for the first point light's six directions.
    constexpr GLsizei pointShadowMapSize = 1024;
    constexpr float pointShadowFarPlane = 25.0f;
    GLuint pointShadowFramebuffer = 0;
    GLuint pointShadowTexture = 0;

    glGenFramebuffers(1, &pointShadowFramebuffer);
    glGenTextures(1, &pointShadowTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowTexture);
    for (int face = 0; face < 6; ++face) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_DEPTH_COMPONENT,
            pointShadowMapSize,
            pointShadowMapSize,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Tell the sampler to read from texture unit 0.
    glUseProgram(shaderProgram);
    glUniform1i(textureLocation, 0);
    glUniform1i(normalMapLocation, 3);
    glUniform1i(shadowMapLocation, 1);
    glUniform1i(pointShadowMapLocation, 2);
    glUniform1f(pointShadowFarPlaneLocation, pointShadowFarPlane);
    glUniform3f(lightPositionLocation, 2.0f, 2.0f, 2.0f);
    glUniform3f(lightPosition2Location, -2.0f, 1.0f, 0.0f);
    glUniform3f(lightColorLocation, 1.0f, 1.0f, 1.0f);
    glUniform3f(lightColor2Location, 0.25f, 0.45f, 1.0f);
    glUniform3f(spotLightColorLocation, 1.0f, 1.0f, 1.0f);
    glUniform1f(
        spotInnerCutoffLocation,
        glm::cos(glm::radians(12.5f))
    );
    glUniform1f(
        spotOuterCutoffLocation,
        glm::cos(glm::radians(17.5f))
    );
    glUniform1f(specularStrengthLocation, 0.35f);
    glUniform1f(shininessLocation, 32.0f);
    glUseProgram(debugProgram);
    glUniform1i(debugDepthMapLocation, 0);

    // 9. Set up a first-person camera.
    glm::vec3 cameraPosition(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;
    double lastMouseX = 640.0;
    double lastMouseY = 360.0;
    bool firstMouseSample = true;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    float lastFrameTime = 0.0f;

    // 10. Render one frame repeatedly until the window closes.
    while (!glfwWindowShouldClose(window)) {
        const float currentFrameTime = static_cast<float>(glfwGetTime());
        const float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Keyboard movement uses deltaTime so speed is frame-rate independent.
        const float cameraSpeed = 2.5f * deltaTime;
        const glm::vec3 cameraRight =
            glm::normalize(glm::cross(cameraFront, worldUp));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            cameraPosition += cameraSpeed * cameraFront;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            cameraPosition -= cameraSpeed * cameraFront;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            cameraPosition -= cameraSpeed * cameraRight;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            cameraPosition += cameraSpeed * cameraRight;
        }

        // Mouse movement changes yaw and pitch.
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (firstMouseSample) {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouseSample = false;
        }

        const float xOffset = static_cast<float>(mouseX - lastMouseX);
        const float yOffset = static_cast<float>(lastMouseY - mouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        constexpr float mouseSensitivity = 0.1f;
        yaw += xOffset * mouseSensitivity;
        pitch += yOffset * mouseSensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        glm::vec3 newFront;
        newFront.x = glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
        newFront.y = glm::sin(glm::radians(pitch));
        newFront.z = glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
        cameraFront = glm::normalize(newFront);

        glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Build the Model, View, and Projection transforms for this frame.
        const float time = currentFrameTime;
        const glm::vec3 lightPosition(
            2.0f * glm::cos(time),
            1.5f,
            2.0f * glm::sin(time)
        );
        const glm::vec3 lightPosition2(
            -2.0f * glm::cos(time),
            0.75f,
            -2.0f * glm::sin(time)
        );
        const SceneNode sceneNodes[] = {
            // Root node: all children inherit this rotation.
            {
                glm::rotate(
                    glm::mat4(1.0f),
                    time,
                    glm::vec3(0.5f, 1.0f, 0.0f)
                ),
                -1
            },
            // Child node: local offset and scale are relative to the root.
            {
                glm::translate(
                    glm::mat4(1.0f),
                    glm::vec3(-1.4f, -0.05f, 0.0f)
                ) * glm::rotate(
                    glm::mat4(1.0f),
                    -0.8f * time,
                    glm::vec3(0.0f, 1.0f, 0.5f)
                ) * glm::scale(
                    glm::mat4(1.0f),
                    glm::vec3(0.65f)
                ),
                0
            },
            // Another child node with a different local transform.
            {
                glm::translate(
                    glm::mat4(1.0f),
                    glm::vec3(1.4f, 0.15f, -0.6f)
                ) * glm::rotate(
                    glm::mat4(1.0f),
                    1.3f * time,
                    glm::vec3(1.0f, 0.0f, 0.5f)
                ) * glm::scale(
                    glm::mat4(1.0f),
                    glm::vec3(0.45f)
                ),
                0
            }
        };
        glm::mat4 worldTransforms[3];
        for (int nodeIndex = 0; nodeIndex < 3; ++nodeIndex) {
            const int parentIndex = sceneNodes[nodeIndex].parentIndex;
            worldTransforms[nodeIndex] = parentIndex < 0
                ? sceneNodes[nodeIndex].localTransform
                : worldTransforms[parentIndex]
                    * sceneNodes[nodeIndex].localTransform;
        }
        const glm::mat4 view = glm::lookAt(
            cameraPosition,
            cameraPosition + cameraFront,
            worldUp
        );
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            1280.0f / 720.0f,
            0.1f,
            100.0f
        );
        const glm::mat4 floorModel = glm::mat4(1.0f);
        const glm::mat4 lightProjection = glm::perspective(
            glm::radians(45.0f),
            1.0f,
            0.1f,
            20.0f
        );
        const glm::mat4 lightView = glm::lookAt(
            cameraPosition,
            cameraPosition + cameraFront,
            worldUp
        );
        const glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // Pass 1: render only depth from the spotlight's point of view.
        glViewport(0, 0, shadowMapWidth, shadowMapHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(depthProgram);
        glUniformMatrix4fv(
            depthLightSpaceLocation,
            1,
            GL_FALSE,
            glm::value_ptr(lightSpaceMatrix)
        );
        for (const auto& model : worldTransforms) {
            glUniformMatrix4fv(
                depthModelLocation,
                1,
                GL_FALSE,
                glm::value_ptr(model)
            );
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, modelVertexCount);
        }
        glUniformMatrix4fv(
            depthModelLocation,
            1,
            GL_FALSE,
            glm::value_ptr(floorModel)
        );
        glBindVertexArray(floorVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Render six depth faces for the first point light.
        const glm::mat4 pointShadowProjection = glm::perspective(
            glm::radians(90.0f),
            1.0f,
            0.1f,
            pointShadowFarPlane
        );
        const glm::vec3 pointShadowDirections[] = {
            glm::vec3( 1.0f,  0.0f,  0.0f),
            glm::vec3(-1.0f,  0.0f,  0.0f),
            glm::vec3( 0.0f,  1.0f,  0.0f),
            glm::vec3( 0.0f, -1.0f,  0.0f),
            glm::vec3( 0.0f,  0.0f,  1.0f),
            glm::vec3( 0.0f,  0.0f, -1.0f)
        };
        const glm::vec3 pointShadowUps[] = {
            glm::vec3(0.0f, -1.0f,  0.0f),
            glm::vec3(0.0f, -1.0f,  0.0f),
            glm::vec3(0.0f,  0.0f,  1.0f),
            glm::vec3(0.0f,  0.0f, -1.0f),
            glm::vec3(0.0f, -1.0f,  0.0f),
            glm::vec3(0.0f, -1.0f,  0.0f)
        };

        glViewport(0, 0, pointShadowMapSize, pointShadowMapSize);
        glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFramebuffer);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glUseProgram(depthProgram);

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                pointShadowTexture,
                0
            );
            glClear(GL_DEPTH_BUFFER_BIT);

            const glm::mat4 faceView = glm::lookAt(
                lightPosition,
                lightPosition + pointShadowDirections[face],
                pointShadowUps[face]
            );
            const glm::mat4 faceLightSpace =
                pointShadowProjection * faceView;

            glUniformMatrix4fv(
                depthLightSpaceLocation,
                1,
                GL_FALSE,
                glm::value_ptr(faceLightSpace)
            );
            for (const auto& model : worldTransforms) {
                glUniformMatrix4fv(
                    depthModelLocation,
                    1,
                    GL_FALSE,
                    glm::value_ptr(model)
                );
                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, modelVertexCount);
            }

            glUniformMatrix4fv(
                depthModelLocation,
                1,
                GL_FALSE,
                glm::value_ptr(floorModel)
            );
            glBindVertexArray(floorVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Pass 2: render the normal scene and sample the shadow map.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, 1280, 720);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(
            lightSpaceMatrixLocation,
            1,
            GL_FALSE,
            glm::value_ptr(lightSpaceMatrix)
        );

        // The camera can move, so update the view position every frame.
        glUniform3fv(
            cameraPositionLocation,
            1,
            glm::value_ptr(cameraPosition)
        );

        // The spotlight follows the camera like a flashlight.
        glUniform3fv(
            spotLightPositionLocation,
            1,
            glm::value_ptr(cameraPosition)
        );
        glUniform3fv(
            spotLightDirectionLocation,
            1,
            glm::value_ptr(cameraFront)
        );

        // Move the point light around the cube once per frame.
        glUniform3fv(
            lightPositionLocation,
            1,
            glm::value_ptr(lightPosition)
        );
        glUniform3fv(
            lightPosition2Location,
            1,
            glm::value_ptr(lightPosition2)
        );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTexture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, normalTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(vao);
        for (const auto& model : worldTransforms) {
            const glm::mat4 transform = projection * view * model;
            glUniformMatrix4fv(
                transformLocation,
                1,
                GL_FALSE,
                glm::value_ptr(transform)
            );
            glUniformMatrix4fv(
                modelLocation,
                1,
                GL_FALSE,
                glm::value_ptr(model)
            );

            for (const auto& part : modelParts) {
                GLuint partTexture = texture;
                const auto textureIt = materialTextures.find(
                    part.diffuseTextureName
                );
                if (textureIt != materialTextures.end()) {
                    partTexture = textureIt->second;
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, partTexture);
                glUniform3fv(
                    materialColorLocation,
                    1,
                    glm::value_ptr(part.diffuseColor)
                );
                glDrawArrays(
                    GL_TRIANGLES,
                    part.firstVertex,
                    part.vertexCount
                );
            }
        }

        // Draw the floor with its own model matrix.
        const glm::mat4 floorTransform = projection * view * floorModel;
        glUniformMatrix4fv(
            transformLocation,
            1,
            GL_FALSE,
            glm::value_ptr(floorTransform)
        );
        glUniformMatrix4fv(
            modelLocation,
            1,
            GL_FALSE,
            glm::value_ptr(floorModel)
        );
        glUniform3f(materialColorLocation, 1.0f, 1.0f, 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(floorVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Draw the light as a small glowing point so its position is visible.
        const glm::mat4 viewProjection = projection * view;
        glDisable(GL_DEPTH_TEST);
        glUseProgram(lightProgram);
        glUniformMatrix4fv(
            lightTransformLocation,
            1,
            GL_FALSE,
            glm::value_ptr(viewProjection)
        );
        glUniform3fv(
            markerLightPositionLocation,
            1,
            glm::value_ptr(lightPosition)
        );
        glUniform3f(markerColorLocation, 1.0f, 0.85f, 0.2f);
        glDrawArrays(GL_POINTS, 0, 1);

        glUniform3fv(
            markerLightPositionLocation,
            1,
            glm::value_ptr(lightPosition2)
        );
        glUniform3f(markerColorLocation, 0.25f, 0.45f, 1.0f);
        glDrawArrays(GL_POINTS, 0, 1);
        glEnable(GL_DEPTH_TEST);

        // Hold F1 to inspect the 2D spotlight shadow map directly.
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
            glDisable(GL_DEPTH_TEST);
            glUseProgram(debugProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadowTexture);
            glBindVertexArray(debugVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 11. Release OpenGL resources and close GLFW.
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &floorVao);
    glDeleteBuffers(1, &floorVbo);
    glDeleteVertexArrays(1, &debugVao);
    glDeleteBuffers(1, &debugVbo);
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &normalTexture);
    for (const auto& [name, materialTexture] : materialTextures) {
        glDeleteTextures(1, &materialTexture);
    }
    glDeleteTextures(1, &shadowTexture);
    glDeleteFramebuffers(1, &shadowFramebuffer);
    glDeleteTextures(1, &pointShadowTexture);
    glDeleteFramebuffers(1, &pointShadowFramebuffer);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(lightProgram);
    glDeleteProgram(depthProgram);
    glDeleteProgram(debugProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

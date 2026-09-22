#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cmath>

#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "OpenGL Practice", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create the GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to load OpenGL functions through GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL version: " << GLAD_VERSION_MAJOR(version)
              << "." << GLAD_VERSION_MINOR(version) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    const char* vertexShaderSource = R"(
        #version 330 core

        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        uniform vec2 uOffset;
        uniform float uAngle;
        out vec3 vertexColor;

    void main() {
        float c = cos(uAngle);
    float s = sin(uAngle);

    mat2 rotation = mat2(
        c, -s,
        s,  c
    );

    vec2 rotatedPosition = rotation * aPos.xy;

    gl_Position = vec4(
        rotatedPosition + uOffset,
        aPos.z,
        1.0
    );

    vertexColor = aColor;
    }
    )";

    const char* fragmentShaderSource = R"(
    #version 330 core

    in vec3 vertexColor;
    out vec4 FragColor;

    void main() {
        FragColor = vec4(vertexColor, 1.0);
    }
    )";
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    int offsetLocation =
        glGetUniformLocation(shaderProgram, "uOffset");
    int angleLocation =
        glGetUniformLocation(shaderProgram, "uAngle");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    float vertices[] = {
        // 位置                  // 颜色
        -0.5f, -0.5f, 0.0f,      1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,      0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f,      0.0f, 0.0f, 1.0f
    };

    unsigned int vao;
    unsigned int vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
		GL_FLOAT,
		GL_FALSE,
		6 * sizeof(float),
		nullptr
    );
    glEnableVertexAttribArray(0);

    // 颜色：每个顶点位置之后的 3 个 float
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.08f, 0.12f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        float time = static_cast<float>(glfwGetTime());
        glUniform1f(angleLocation, time);
        float offsetX = std::sin(time) * 0.5f;

        glUniform2f(offsetLocation, offsetX, 0.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

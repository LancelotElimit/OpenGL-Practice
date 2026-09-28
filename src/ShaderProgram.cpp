#include "ShaderProgram.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool checkShader(GLuint shader, const char* name) {
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return true;
    }

    char infoLog[2048]{};
    glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
    std::cerr << name << " compilation failed:\n" << infoLog << '\n';
    return false;
}

bool checkProgram(GLuint program, const char* name) {
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_TRUE) {
        return true;
    }

    char infoLog[2048]{};
    glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
    std::cerr << name << " linking failed:\n" << infoLog << '\n';
    return false;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open shader file: " << path << '\n';
        return {};
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

} // namespace

ShaderProgram::ShaderProgram(
    const char* vertexSource,
    const char* fragmentSource,
    const char* debugName
) {
    build(vertexSource, fragmentSource, debugName);
}

ShaderProgram::ShaderProgram(
    const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath,
    const char* debugName
) {
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        return;
    }

    build(vertexSource.c_str(), fragmentSource.c_str(), debugName);
}

void ShaderProgram::build(
    const char* vertexSource,
    const char* fragmentSource,
    const char* debugName
) {
    const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    if (!checkShader(vertexShader, debugName)) {
        glDeleteShader(vertexShader);
        return;
    }

    const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    if (!checkShader(fragmentShader, debugName)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!checkProgram(program_, debugName)) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_(other.program_) {
    other.program_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (program_ != 0) {
        glDeleteProgram(program_);
    }
    program_ = other.program_;
    other.program_ = 0;
    return *this;
}

bool ShaderProgram::valid() const {
    return program_ != 0;
}

GLuint ShaderProgram::id() const {
    return program_;
}

void ShaderProgram::use() const {
    glUseProgram(program_);
}

GLint ShaderProgram::uniform(const char* name) const {
    return glGetUniformLocation(program_, name);
}

void ShaderProgram::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

std::string Shader::readFile(const std::string& path)
{
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Couldn't open " + path);

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const std::string& source)
{
    GLuint shader = glCreateShader(type);

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);

        std::cerr << log << '\n';
        throw std::runtime_error("Shader compilation failed.");
    }

    return shader;
}

Shader::Shader(const std::string& vertexPath,
               const std::string& fragmentPath)
{
    std::string vertexSource = readFile(vertexPath);
    std::string fragmentSource = readFile(fragmentPath);

    GLuint vs = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSource);

    program = glCreateProgram();

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);

        std::cerr << log << '\n';
        throw std::runtime_error("Program linking failed.");
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader()
{
    glDeleteProgram(program);
}

void Shader::use() const
{
    glUseProgram(program);
}
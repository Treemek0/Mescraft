#include "shader.h"

unsigned int make_shader(const std::string& vertex_path, const std::string& fragment_path){
    std::vector<unsigned int> shaders;
    shaders.push_back(make_module(vertex_path, GL_VERTEX_SHADER));
    shaders.push_back(make_module(fragment_path, GL_FRAGMENT_SHADER));

    unsigned int program = glCreateProgram();
    for(unsigned int shader : shaders){
        glAttachShader(program, shader);
    }
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    for(unsigned int shader : shaders){
        glDeleteShader(shader);
    }

    return program;
}

unsigned int make_module(const std::string& path, unsigned int type){
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << path << std::endl;
        // You might want to return a specific error code or handle this more gracefully
        return 0;
    }

    std::string line;
    std::stringstream bufferedLines;

    while(std::getline(file, line)){
        bufferedLines << line << "\n";
    }
    
    std::string shaderSource = bufferedLines.str();
    const char* source = shaderSource.c_str();
    bufferedLines.clear();
    file.close();
    unsigned int shaderModule = glCreateShader(type);
    glShaderSource(shaderModule, 1, &source, NULL);
    glCompileShader(shaderModule);

    int success;
    glGetShaderiv(shaderModule, GL_COMPILE_STATUS, &success);
    if(!success){
        char infoLog[512];
        glGetShaderInfoLog(shaderModule, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shaderModule;
}
#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

enum class TextureWrap : int {
    REPEAT         = GL_REPEAT,          // Repeats the texture (tile)
    MIRRORED_REPEAT = GL_MIRRORED_REPEAT, // Repeats the texture but mirrors each repeat
    CLAMP_TO_EDGE  = GL_CLAMP_TO_EDGE,   // Clamps UVs to edge pixels
    CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER // Uses a border color for out-of-range UVs
};

enum class TextureFilter : int {
    NEAREST               = GL_NEAREST,               // Use nearest texel (pixelated)
    LINEAR                = GL_LINEAR,                // Linear interpolation (smooth)
    NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST, // Nearest mipmap, nearest texel
    LINEAR_MIPMAP_NEAREST  = GL_LINEAR_MIPMAP_NEAREST,  // Nearest mipmap, linear interpolation
    NEAREST_MIPMAP_LINEAR  = GL_NEAREST_MIPMAP_LINEAR,  // Linear mipmap interpolation, nearest texel
    LINEAR_MIPMAP_LINEAR   = GL_LINEAR_MIPMAP_LINEAR    // Linear mipmap interpolation, linear texel interpolation
};

class TextureManager {
public:
    std::vector<unsigned int> textures;
    
    unsigned int loadTexture(const char* filename, TextureWrap wrapX, TextureWrap wrapY, TextureFilter downFilter, TextureFilter upFilter);
    void deleteTextures();
};
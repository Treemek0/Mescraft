#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <glad/glad.h>
#include <iostream>
#include "textureManager.h"

unsigned int TextureManager::loadTexture(const char* filename, TextureWrap wrapX, TextureWrap wrapY, TextureFilter downFilter, TextureFilter upFilter) {
        int width, height, channels;
        unsigned char* data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
        if (data == NULL) {
            std::cout << "Failed to load texture: " << filename << std::endl;
            return 0;
        }
        
        //make the texture
        unsigned int texture;
        glGenTextures(1, &texture);
        textures.push_back(texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        //load data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        // Generate mipmaps for smoother textures at a distance
        glGenerateMipmap(GL_TEXTURE_2D);

        //free data
        stbi_image_free(data);

        //Configure sampler
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(wrapX));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(wrapY));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<int>(downFilter)); // down scaling image
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<int>(upFilter)); // up scaling image

        return texture;
    }

    void TextureManager::deleteTextures() {
        glDeleteTextures(textures.size(), textures.data());
        textures.clear();
    }
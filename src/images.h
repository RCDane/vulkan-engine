#pragma once

#include "fmt/core.h"
#include "glm/common.hpp"
#include "stb_image.h"
#include <filesystem>
#include <string_view>
#include <vector>



enum IMAGE_TYPE {
    RGB8,
    RGBA8,
    RGB16,
    RGBA16,
    RGB32,
    RGBA32
};

struct Image {
    int width;
    int height;
    IMAGE_TYPE type;
    char* data;
};




Image load_image(std::string_view filepath)
{
    uint32_t width, height, nrChannels;

    

    unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 4);
    VkDeviceSize imageSize = width * height * 4;
}

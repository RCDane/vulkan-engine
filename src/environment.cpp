#include "environment.h"
#include <cstdint>


environment_map create_environment_map(VulkanEngine* engine, std::string_view filePath){
    // load environment map 
    const std::string suffixes[] = { "right", "left", "top", "bottom", "front", "back" };
    int width, height, nrChannels;

    std::filesystem::path path = fmt::format("{}/{}.png", filePath, suffixes[0]);
    auto stringPath = path.string();

    unsigned char* data = stbi_load(stringPath.c_str(), &width, &height, &nrChannels, 4);
    uint32_t imageSize = width * height * 4;
	uint32_t cubeImageSize = width * height * 4 * 6;


    // calculate alias
}
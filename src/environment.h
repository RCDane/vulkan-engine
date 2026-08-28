#pragma once
#include "stb_image.h"

#include "vk_engine.h"
#include "vk_images.h"
#include "vk_types.h"
#include <cstdint>


// Flow for env_map should be load -> preprocess -> load into buffers -> add to GPU.  

struct alias_triplet {
    float treshold;
    uint32_t index;
    float pdf;
};

struct environment_map{
    AllocatedBuffer gpu_buffer;
    std::string name;
};


environment_map create_environment_map(VulkanEngine* engine, std::string_view filePath);
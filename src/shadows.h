#pragma once
#ifndef SHADOWS_H

#define SHADOWS_H

#include "glm/ext/vector_float3.hpp"
#include "vk_types.h"
#include <vk_images.h>
#include <glm/common.hpp>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

class VulkanEngine;

struct DirectionalShadow {
public:
    glm::vec3 direction;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};

struct DirectionalLighting {
    glm::vec3 direction;
    float intensity;
    glm::vec4 color;
	glm::mat4 lightView;
};


DirectionalShadow prepare_directional_shadow(VulkanEngine* engine, DrawContext scene, glm::vec3 direction);

struct ShadowPipeline{
    // Needs Layout
    // Needs pipeline
    // Need to know resources
    // Needs to know image
    // Needs to be able to debug.
    VkPipeline pipeline;
    VkPipelineLayout layout;
    GPUDrawPushConstants pushConstants;

    void build_pipelines(VulkanEngine* engine);
    void prepare_images(VulkanEngine* engine);
};


struct ShadowImage {
    AllocatedImage image;
    VkSampler sampler;
    VkExtent3D resolution;

    void prepare_image(VkExtent3D resolution ,VulkanEngine* engine);
};


#endif // !SHADOWS_H
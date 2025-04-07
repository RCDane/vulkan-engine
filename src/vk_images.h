
#pragma once 

#include <vulkan/vulkan.h>
#include <vk_types.h>



namespace vkutil {
    void transition_depth(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);
    void transition_swapchain(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void transition_image_relaxed(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);
    void transition_shadow_map(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);	
    void transition_gbuffer_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void transition_gbuffer_images(VkCommandBuffer cmd, const std::vector<VkImage>& images, VkImageLayout oldLayout, VkImageLayout newLayout);

    void transition_main_color_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};
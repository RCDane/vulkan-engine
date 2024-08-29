
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <shadows.h>
#include <vk_images.h>
#include <vk_engine.h>
#include <vk_pipelines.h>
#include <vulkan/vulkan_core.h>


void ShadowPipeline::build_pipelines(VulkanEngine* engine)
{
	// Prepare shaders
    VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("../../shaders/shadow.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("../../shaders/shadow.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

	// Prepare layout
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	VkDescriptorSetLayout layout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT);

	VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info();
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &layout;
	layout_info.pPushConstantRanges = & matrixRange;
	layout_info.pushConstantRangeCount = 1;
	
	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &layout_info, nullptr, &newLayout));
	
	this->layout = newLayout;



	// PreparePipeline
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.set_depth_format(VK_FORMAT_D32_SFLOAT);

	pipelineBuilder._pipelineLayout = newLayout;

	VkPipeline pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);	


}

void ShadowImage::prepare_image(VkExtent3D resolution, VulkanEngine* engine){
	this->resolution = resolution;
	
	// TODO: Find out which usages are necessar
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	image = engine->create_image(this->resolution, VK_FORMAT_D32_SFLOAT, drawImageUsages, false);

	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;

	vkCreateSampler(engine->_device, &sampl, nullptr, &this->sampler);

}


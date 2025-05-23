
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <shadows.h>
#include <vk_images.h>
#include <vk_engine.h>
#include <vk_pipelines.h>
#include <vulkan/vulkan_core.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>


void ShadowPipeline::build_pipelines(VulkanEngine* engine)
{
	// Prepare shaders
    VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("shaders/spv/shadow.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("shaders/spv/shadow.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

	// Prepare layout
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	VkDescriptorSetLayout layout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info();
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &layout;
	layout_info.pPushConstantRanges = &matrixRange;
	layout_info.pushConstantRangeCount = 1;
	
	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &layout_info, nullptr, &newLayout));
	
	this->layout = newLayout;



	// PreparePipeline
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.set_depth_format(VK_FORMAT_D32_SFLOAT);
	pipelineBuilder.set_multisampling_none();

	pipelineBuilder._pipelineLayout = newLayout;

	this->pipeline = pipelineBuilder.build_pipeline(engine->_device);
	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);	


}

void ShadowImage::prepare_image(VkExtent3D resolution, VulkanEngine* engine){
	this->resolution = resolution;
	
	// TODO: Find out which usages are necessar
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	 drawImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	image = engine->create_image(this->resolution, VK_FORMAT_D32_SFLOAT, drawImageUsages, false);
	
	VkSamplerCreateInfo samplerInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.compareEnable = VK_TRUE;
	samplerInfo.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;


	vkCreateSampler(engine->_device, &samplerInfo, nullptr, &this->sampler);
	engine->immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image(cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,VK_IMAGE_ASPECT_DEPTH_BIT);
		});


	// Descriptor layout for fragment shader
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	engine->_shadowDescriptorLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

	

}


DirectionalShadow prepare_directional_shadow(VulkanEngine* engine, DrawContext scene, glm::vec3 direction) {
	std::vector<RenderObject> opaque = scene.OpaqueSurfaces;
	glm::vec3 minBounds = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
	for (const RenderObject& obj : opaque) {
		glm::vec3 objMin = obj.bounds.origin - obj.bounds.extents;
		glm::vec3 objMax = obj.bounds.origin + obj.bounds.extents;
		minBounds = glm::min(minBounds, objMin);
		maxBounds = glm::max(maxBounds, objMax);
	}
	glm::vec3 middle = (minBounds + maxBounds) / 2.0f;

	// Light is pointing from the light towards the scene.
	// Ne therefore negate the light direction such that we can move the camera towards the light.
	direction = -glm::normalize(direction);
	glm::vec3 basis_up;
	if (fabs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f) {
		basis_up = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	else {
		basis_up = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	glm::vec3 basis_right = glm::cross(direction, basis_up);
	glm::vec3 newUp = glm::cross(basis_right, direction);
	// Light view matrix (similar to a camera view matrix)

	glm::mat4 viewMatrix = glm::lookAt(direction, glm::vec3(0.0f), newUp);

	// Initialize orthographic bounds with large limits
	float near = std::numeric_limits<float>::max();
	float far = -std::numeric_limits<float>::max();
	float top = -std::numeric_limits<float>::max();
	float bottom = std::numeric_limits<float>::max();
	float left = std::numeric_limits<float>::max();
	float right = -std::numeric_limits<float>::max();

	// Calculate the orthographic projection bounds based on the scene contents
	for (const RenderObject& obj : opaque) {
		Bounds bounds = obj.bounds;
		glm::vec3 center = bounds.origin;
		glm::vec3 extents = bounds.extents;

		// Generate the 8 corners of the bounding box
		std::array<glm::vec3, 8> corners{

			center + glm::vec3(-extents.x, -extents.y, -extents.z),
			center + glm::vec3(extents.x, -extents.y, -extents.z),
			center + glm::vec3(extents.x, extents.y, -extents.z),
			center + glm::vec3(-extents.x, extents.y, -extents.z),
			center + glm::vec3(-extents.x, -extents.y, extents.z),
			center + glm::vec3(extents.x, -extents.y, extents.z),
			center + glm::vec3(extents.x, extents.y, extents.z),
			center + glm::vec3(-extents.x, extents.y, extents.z)
		};

		// Transform each corner into light space
		for (const glm::vec3& corner : corners) {
			glm::vec4 v = viewMatrix  * obj.transform * glm::vec4(corner, 1.0f);

			

			// Update orthographic projection bounds
			left = std::min(left, v.x);
			right = std::max(right, v.x);
			bottom = std::min(bottom, v.y);
			top = std::max(top, v.y);
			near = std::min(near, v.z);
			far = std::max(far, v.z);
		}
	}

	
	float offset = (far - near);
	float correctedFar = (far - near);
	glm::mat4 projectionMatrix = glm::orthoRH_ZO(left, right, bottom, top, 0.001f, (correctedFar+1.0f)*1.5f);
	


	glm::vec3 eyePosition =  direction * (near);
	glm::vec3 lookAtPosition = direction + direction * offset;
	viewMatrix = glm::lookAt(eyePosition, eyePosition + direction, newUp);
	engine->_directionalLighting.lightView =  projectionMatrix* viewMatrix;

	
	// Create and return the DirectionalShadow struct
	DirectionalShadow shadow;
	shadow.direction = direction;
	shadow.viewMatrix = viewMatrix; // Move camera back to ensure that scene is encased in view frustrum
	shadow.projectionMatrix = projectionMatrix;
	return shadow;
}



#include <vk_engine.h>
#include <svgf.h>
#include <vk_pipelines.h>
#include <vk_descriptors.h>

void SVGFHandler::create_reprojection_pipeline(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_reprojDscSetLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(SVGFSettings);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &computeLayout, nullptr, &m_reprojPipelineLayout));


	VkShaderModule toneMappingShader;
	if (!vkutil::load_shader_module("../shaders/spv/svgf_reproj.comp.spv", engine->_device, &toneMappingShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = toneMappingShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_reprojPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_reprojectionPipeline));


	//destroy structures properly
	vkDestroyShaderModule(engine->_device, toneMappingShader, nullptr);
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, m_reprojPipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, m_reprojectionPipeline, nullptr);
		});
}

void SVGFHandler::create_frame_buffers(VulkanEngine* engine) {
	VkImageUsageFlags reprojectionUsages{};
	reprojectionUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkExtent3D extent{ engine->_drawExtent.width, engine->_drawExtent.height, 1};

	NormalAndDepth.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	NormalAndDepth.imageExtent = extent;

	engine->create_render_buffer(NormalAndDepth, reprojectionUsages, VK_IMAGE_ASPECT_COLOR_BIT);
}

void SVGFHandler::create_descriptors(VulkanEngine* engine) {
	DescriptorLayoutBuilder builder;

	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFWidthZWidth
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // depth
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // albedo
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevIllumination
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevDepth
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevNormalFWidthZWidth
	builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevHistoryLength
    m_reprojDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);

	builder.clear();
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(engine->_device, m_reprojDscSetLayout, nullptr);
		});
}

void SVGFHandler::init(VulkanEngine* engine, VkExtent2D extent) {
	// Create buffers that are not owned by engine

	// Create descriptor sets

	// Create pipelines (mainly compute shaders)

	// 
}

SVGFHandler::SVGFHandler(VulkanEngine* engine, VkExtent2D extent) {
	init(engine, extent);
}



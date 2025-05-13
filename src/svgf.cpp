#include <vk_engine.h>
#include <svgf.h>
#include <vk_pipelines.h>
#include <vk_descriptors.h>
#include <vk_images.h>
void SVGFHandler::create_reprojection_pipeline(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_reprojDscSetLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstantReproj);
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

	VkImageUsageFlags transferUsages{};
	transferUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	transferUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	transferUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkExtent3D extent{ engine->_windowExtent.width, engine->_windowExtent.height, 1};

	normalFWidthZWidth.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	normalFWidthZWidth.imageExtent = extent;

	engine->create_render_buffer(normalFWidthZWidth, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);

	prevNormalFWidthZWidth.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	prevNormalFWidthZWidth.imageExtent = extent;

	engine->create_render_buffer(prevNormalFWidthZWidth, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);

	illumination.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	illumination.imageExtent = extent;

	engine->create_render_buffer(illumination, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevHistoryLength.imageFormat = VK_FORMAT_R8_UINT;
	prevHistoryLength.imageExtent = extent;

	engine->create_render_buffer(prevHistoryLength, reprojectionUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevIllumination.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	prevIllumination.imageExtent = extent;

	engine->create_render_buffer(prevIllumination, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevMoments.imageFormat = VK_FORMAT_R16G16_SFLOAT;
	prevMoments.imageExtent = extent;

	engine->create_render_buffer(prevMoments, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);
}

void SVGFHandler::create_descriptors(VulkanEngine* engine) {
	DescriptorLayoutBuilder builder;

	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFWidthZWidth
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // depth
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // albedo
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevIllumination
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // prevDepth
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevNormalFWidthZWidth
	builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevHistoryLength
	builder.add_binding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevMoments
	builder.add_binding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // illumination

    m_reprojDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);

	builder.clear();
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(engine->_device, m_reprojDscSetLayout, nullptr);
		});
}

void SVGFHandler::init(VulkanEngine* engine, VkExtent2D extent) {
	create_frame_buffers(engine);
	create_descriptors(engine);
	create_reprojection_pipeline(engine);
}

SVGFHandler::SVGFHandler(VulkanEngine* engine, VkExtent2D extent) {
	init(engine, extent);
}

void SVGFHandler::Reprojection(VkCommandBuffer cmd, VulkanEngine* engine) {
	
	
	
	
	vkutil::transition_image(cmd, normalFWidthZWidth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevIllumination.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevNormalFWidthZWidth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevHistoryLength.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMoments.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	
	vkutil::transition_image(cmd, engine->_depthImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	vkutil::transition_image(cmd, engine->_depthHistory.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_reprojectionPipeline);







	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_reprojDscSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, engine->_gBuffer_normal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, engine->_colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, engine->_depthImage.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, engine->_gBuffer_albedo.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(4, prevIllumination.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(5, engine->_depthHistory.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(6, prevNormalFWidthZWidth.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(7, prevHistoryLength.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(8, prevMoments.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(9, illumination.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	
	
	writer.update_set(engine->_device, globalDescriptor);


	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_reprojPipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);


	PushConstantReproj settings;
	settings.imageSize = glm::ivec2(engine->_windowExtent.width, engine->_windowExtent.height);
	settings.viewInverse = glm::inverse(engine->mainCamera->getViewMatrix());
	settings.projInverse = glm::inverse(engine->mainCamera->getProjectionMatrix());
	settings.oldViewProj = engine->_raytracingHandler.m_uniformMappedPtr->oldViewProj;




	vkCmdPushConstants(cmd, m_reprojPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantReproj), &settings);


	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(engine->_windowExtent.width / 16.0), std::ceil(engine->_windowExtent.height / 16.0), 1);

	// Copy necessary image history
	vkutil::transition_image(cmd, engine->_gBuffer_normal.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevNormalFWidthZWidth.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::copy_image_to_image(cmd, engine->_gBuffer_normal.image, prevNormalFWidthZWidth.image, engine->_windowExtent, engine->_windowExtent);

	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevIllumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::copy_image_to_image(cmd, illumination.image, prevIllumination.image, engine->_windowExtent, engine->_windowExtent);

}

void SVGFHandler::Execute(VkCommandBuffer cmd, VulkanEngine *engine) {
	// PackNormalDepth(cmd);
	Reprojection(cmd, engine);
	//FilterMoments(cmd);
	//WaveletFilter(cmd);
}

SVGFHandler::SVGFHandler() {

}

SVGFHandler::~SVGFHandler() {
	//destroy structures properly
	//vkDestroyImageView(engine->_device, illuminatuion.imageView, nullptr);
	//vkDestroyImageView(engine->_device, prevIllumination.imageView, nullptr);
	//vkDestroyImageView(engine->_device, NormalAndDepth.imageView, nullptr);
	//vkDestroyImageView(engine->_device, prevNormalAndDepth.imageView, nullptr);
	//vkDestroyImageView(engine->_device, historyLength.imageView, nullptr);
	//vkDestroyImageView(engine->_device, prevHistoryLength.imageView, nullptr);
}
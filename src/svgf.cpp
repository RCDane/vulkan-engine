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


void SVGFHandler::create_moments_filter_pipeline(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_momentsFilterDscSetLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstantImageSize);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &computeLayout, nullptr, &m_momentsFilterPipelineLayout));


	VkShaderModule momentsFilterShader;
	if (!vkutil::load_shader_module("../shaders/spv/svgfFilterMoments.comp.spv", engine->_device, &momentsFilterShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = momentsFilterShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_momentsFilterPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_momentsFilterPipeline));


	//destroy structures properly
	vkDestroyShaderModule(engine->_device, momentsFilterShader, nullptr);
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, m_momentsFilterPipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, m_momentsFilterPipeline, nullptr);
		});
}

void SVGFHandler::create_wavelet_filter_pipeline(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_atrousDscSetLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstantAtrous);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &computeLayout, nullptr, &m_atrousPipelineLayout));


	VkShaderModule AtrousShader;
	if (!vkutil::load_shader_module("../shaders/spv/svgfAtrous.comp.spv", engine->_device, &AtrousShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = AtrousShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_atrousPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_atrousPipeline));


	//destroy structures properly
	vkDestroyShaderModule(engine->_device, AtrousShader, nullptr);
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, m_atrousPipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, m_atrousPipeline, nullptr);
		});
}

void SVGFHandler::create_packing_pipeline(VulkanEngine* engine) {
	// Add creation of descriptor set layout for packing pass
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Output packed image
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Input normal
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Input depth
	m_packNormalDepthDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);
	builder.clear();

	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_packNormalDepthDscSetLayout;
	computeLayout.setLayoutCount = 1;
	computeLayout.pushConstantRangeCount = 0;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &computeLayout, nullptr, &m_NormalDepthPipelineLayout));

	VkShaderModule packingShader;
	if (!vkutil::load_shader_module("../shaders/spv/svgfPacking.comp.spv", engine->_device, &packingShader)) {
		fmt::print("Error when building the packing compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = packingShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_NormalDepthPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_packNormalDepthPipeline));

	//destroy structures properly
	vkDestroyShaderModule(engine->_device, packingShader, nullptr);
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, m_NormalDepthPipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, m_packNormalDepthPipeline, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, m_packNormalDepthDscSetLayout, nullptr);
	});
}


void SVGFHandler::create_modulate_pipeline(VulkanEngine* engine) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_modulateDscSetLayout;
	computeLayout.setLayoutCount = 1;

	computeLayout.pushConstantRangeCount = 0;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &computeLayout, nullptr, &m_modulatePipelineLayout));


	VkShaderModule modulateShader;
	if (!vkutil::load_shader_module("../shaders/spv/modulate.comp.spv", engine->_device, &modulateShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = modulateShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_modulatePipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_modulatePipeline));


	//destroy structures properly
	vkDestroyShaderModule(engine->_device, modulateShader, nullptr);
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, m_modulatePipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, m_modulatePipeline, nullptr);
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

	illuminationBlend.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	illuminationBlend.imageExtent = extent;



	engine->create_render_buffer(illuminationBlend, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevHistoryLength.imageFormat = VK_FORMAT_R8_UINT;
	prevHistoryLength.imageExtent = extent;




	engine->create_render_buffer(prevHistoryLength, reprojectionUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevIllumination.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	prevIllumination.imageExtent = extent;

	engine->create_render_buffer(prevIllumination, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);


	prevMoments.imageFormat = VK_FORMAT_R16G16_SFLOAT;
	prevMoments.imageExtent = extent;

	engine->create_render_buffer(prevMoments, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);

	packedDepthNormal.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	packedDepthNormal.imageExtent = extent;

	engine->create_render_buffer(packedDepthNormal, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	prevPackedDepthNormal.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	prevPackedDepthNormal.imageExtent = extent;

	engine->create_render_buffer(prevPackedDepthNormal, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);

	prevMetalRougness.imageFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	prevMetalRougness.imageExtent = extent;

	engine->create_render_buffer(prevMetalRougness, transferUsages, VK_IMAGE_ASPECT_COLOR_BIT);
}

void SVGFHandler::create_descriptors(VulkanEngine* engine) {
	DescriptorLayoutBuilder builder;

	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFWidthZWidth
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // albedo
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevIllumination
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevNormalFWidthZWidth
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevHistoryLength
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevMoments
	builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // illumination
	builder.add_binding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // metalRougnness
	builder.add_binding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // prevMetalRougness
	builder.add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // prevMetalRougness



    m_reprojDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);

	builder.clear();
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Illumination
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // history
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFwidthZWidth
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // moments
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)


	m_momentsFilterDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);
	builder.clear();


	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Illumination
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // history
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFwidthZWidth
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // albedo
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)
	builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)
	builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // color (debug)




	m_atrousDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);
	builder.clear();

	// modulate
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Illumination
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // history
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // normalFwidthZWidth



	m_modulateDscSetLayout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT, NULL);
	
	
	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(engine->_device, m_reprojDscSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, m_momentsFilterDscSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, m_atrousDscSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, m_packNormalDepthDscSetLayout, nullptr);
		});
}

void SVGFHandler::init(VulkanEngine* engine, VkExtent2D extent) {
	create_frame_buffers(engine);
	create_descriptors(engine);
	create_reprojection_pipeline(engine);
	create_moments_filter_pipeline(engine);
	create_wavelet_filter_pipeline(engine);
	create_modulate_pipeline(engine);
	create_packing_pipeline(engine);
}

SVGFHandler::SVGFHandler(VulkanEngine* engine, VkExtent2D extent) {
	init(engine, extent);
}

void SVGFHandler::Reprojection(VkCommandBuffer cmd, VulkanEngine* engine) {




	vkutil::transition_image(cmd, packedDepthNormal.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, illuminationBlend.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevHistoryLength.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMoments.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevPackedDepthNormal.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMetalRougness.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_metallicRoughnes.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_depthImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);


	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_reprojectionPipeline);







	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_reprojDscSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, packedDepthNormal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, engine->_colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, engine->_gBuffer_albedo.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(3, illuminationBlend.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);	
	writer.write_image(4, prevPackedDepthNormal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(5, prevHistoryLength.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(6, prevMoments.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(7, illumination.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(8, prevMetalRougness.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(9, engine->_gBuffer_metallicRoughnes.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(10, engine->_depthImage.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);



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
	vkutil::transition_image(cmd, packedDepthNormal.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevPackedDepthNormal.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::copy_image_to_image(cmd, packedDepthNormal.image, prevPackedDepthNormal.image, engine->_windowExtent, engine->_windowExtent);


}


void SVGFHandler::FilterMoments(VkCommandBuffer cmd, VulkanEngine* engine) {


	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevHistoryLength.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, packedDepthNormal.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMoments.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_colorHistory.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_metallicRoughnes.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMetalRougness.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

	

	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_momentsFilterDscSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, illumination.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, prevHistoryLength.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, packedDepthNormal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(3, prevMoments.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(4, engine->_colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(5, engine->_gBuffer_metallicRoughnes.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(6, prevMetalRougness.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);



	writer.update_set(engine->_device, globalDescriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_momentsFilterPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_momentsFilterPipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);


	PushConstantImageSize settings;
	settings.imageSize = glm::ivec2(engine->_windowExtent.width, engine->_windowExtent.height);




	vkCmdPushConstants(cmd, m_reprojPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantImageSize), &settings);


	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(engine->_windowExtent.width / 16.0), std::ceil(engine->_windowExtent.height / 16.0), 1);

}



void SVGFHandler::WaveletFilter(VkCommandBuffer cmd, VulkanEngine* engine) {





	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevHistoryLength.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, packedDepthNormal.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_albedo.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevIllumination.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, prevMetalRougness.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_metallicRoughnes.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	std::vector<VkImageLayout> oldLayouts = {
		VK_IMAGE_LAYOUT_GENERAL, 
		VK_IMAGE_LAYOUT_GENERAL, 
		VK_IMAGE_LAYOUT_GENERAL, 
		VK_IMAGE_LAYOUT_GENERAL, 
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL };
	std::vector<VkImageLayout> newLayouts = {
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_GENERAL };

	std::vector<VkImage> images{
		illumination.image,
		prevHistoryLength.image,
		packedDepthNormal.image,
		engine->_gBuffer_albedo.image,
		prevIllumination.image,
		prevMetalRougness.image,
		engine->_gBuffer_metallicRoughnes.image
	};




	vkutil::transition_images_together(cmd, images, oldLayouts, newLayouts);




	AllocatedImage illuminationIn = illumination;
	AllocatedImage illuminationOut = prevIllumination;

	int iterations = 4;

	for (int i = 0; i < iterations; i++) {

		VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_atrousDscSetLayout);


		DescriptorWriter writer;
		writer.write_image(0, illuminationIn.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(1, prevHistoryLength.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(2, packedDepthNormal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(3, engine->_gBuffer_albedo.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(4, illuminationOut.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(5, prevMetalRougness.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(6, engine->_gBuffer_metallicRoughnes.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		writer.update_set(engine->_device, globalDescriptor);


		std::swap(illuminationIn, illuminationOut);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_atrousPipeline);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_atrousPipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);


		PushConstantAtrous settings;
		settings.imageSize = glm::ivec2(engine->_windowExtent.width, engine->_windowExtent.height);
		settings.phiColor = 5;
		settings.phiNormal = 128.0;
		settings.stepSize = 1 << i;



		vkCmdPushConstants(cmd, m_reprojPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantAtrous), &settings);

		vkCmdDispatch(cmd, std::ceil(engine->_windowExtent.width / 16.0), std::ceil(engine->_windowExtent.height / 16.0), 1);
		if (i == 0) {

			vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

			vkutil::transition_image(cmd, illuminationBlend.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkutil::copy_image_to_image(cmd, illumination.image, illuminationBlend.image, engine->_windowExtent, engine->_windowExtent);
			vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


		}
		if (i != iterations)
			vkutil::transition_images_together(cmd, images, newLayouts, newLayouts);


	}

	vkutil::transition_image(cmd, engine->_gBuffer_metallicRoughnes.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image(cmd, prevMetalRougness.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::copy_image_to_image(cmd, engine->_gBuffer_metallicRoughnes.image, prevMetalRougness.image, engine->_windowExtent, engine->_windowExtent);




	illumination = illuminationIn;
	prevIllumination = illuminationOut;
}


void SVGFHandler::Modulate(VkCommandBuffer cmd, VulkanEngine* engine) {
	vkutil::transition_image(cmd, illumination.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_albedo.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_colorHistory.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_modulateDscSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, illumination.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, engine->_gBuffer_albedo.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, engine->_colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.update_set(engine->_device, globalDescriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_modulatePipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_modulatePipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);


	vkCmdDispatch(cmd, std::ceil(engine->_windowExtent.width / 16.0), std::ceil(engine->_windowExtent.height / 16.0), 1);

}

// Add function to execute the packing pass
void SVGFHandler::PackNormalDepth(VkCommandBuffer cmd, VulkanEngine* engine) {
    // Transition output image to general layout
    vkutil::transition_image(cmd, packedDepthNormal.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_gBuffer_normal.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::transition_image(cmd, engine->_depthImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkDescriptorSet descriptorSet = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_packNormalDepthDscSetLayout);
    DescriptorWriter writer;
    writer.write_image(0, packedDepthNormal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.write_image(1, engine->_gBuffer_normal.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.write_image(2, engine->_depthImage.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(engine->_device, descriptorSet);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_packNormalDepthPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_NormalDepthPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDispatch(cmd, std::ceil(engine->_windowExtent.width / 16.0), std::ceil(engine->_windowExtent.height / 16.0), 1);
}

void SVGFHandler::Execute(VkCommandBuffer cmd, VulkanEngine *engine) {


	PackNormalDepth(cmd, engine);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._reprojectionStart);
	Reprojection(cmd, engine);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._reprojectionEnd);

	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._FilterMomentsStart);

	FilterMoments(cmd,engine);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._FilterMomentsEnd);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._waveletStart);
	WaveletFilter(cmd,engine);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._waveletEnd);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._modulateStart);

	Modulate(cmd, engine);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, engine->_timestampQueryPool, engine->get_current_frame()._modulateEnd);

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
// This implementation takes heavy inspiration from the Vulkan Raytracing Tutorial by NVIDIA
// https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/

#pragma once
#ifndef VK_RAYTRACING_H
#define VK_RAYTRACING_H
#include <vk_types.h>
#include <vk_descriptors.h>
class VulkanEngine;  // 
struct RenderObject;
struct DrawContext;


struct BlasInput {
	std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
	VkBuildAccelerationStructureFlagsKHR                  flags{ 0 };
};



class RaytracingBuilder {
public:
	
	BlasInput objectToVkGeometry(VulkanEngine* engine, const RenderObject& object);
	void buildBlas(VulkanEngine* engine, const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags);

protected:
	std::vector<AccelKHR> m_blas;
	DescriptorAllocatorGrowable m_descriptorAllocator;

	bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

};


class RaytracingHandler {
	
private:
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties
		{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	RaytracingBuilder m_rtBuilder;
	DrawContext* m_models;
	
public:
	bool setup(DrawContext &drawContext);
	bool init_raytracing(VulkanEngine *engine);
	void createBottomLevelAS(VulkanEngine *engine);
};




#endif // VK_RAYTRACING_H
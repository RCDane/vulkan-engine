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
	VkDeviceAddress getBlasDeviceAddress(uint32_t blasId, VkDevice device);
	void createTopLevelAS(VulkanEngine* engine, std::vector<RenderObject> models);
	VkAccelerationStructureKHR getAccelerationStructure() const {
		return m_tlas.accel;
	};
	void destroy(VkDevice device);


protected:
	std::vector<AccelKHR> m_blas;
	AccelKHR m_tlas;  // Top-level acceleration structure

	DescriptorAllocatorGrowable m_descriptorAllocator;

	bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }
	void buildTlas(VkCommandBuffer cmd, VulkanEngine* engine,
		const std::vector<VkAccelerationStructureInstanceKHR>& instances,
		VkBuildAccelerationStructureFlagsKHR flags,
		bool                                 update,
		bool                                 motion);
	void cmdCreateTlas(VkCommandBuffer cmdBuf,
		VulkanEngine* engine,
		uint32_t                             countInstance,
		VkDeviceAddress                      instBufferAddr,
		AllocatedBuffer& scratchBuffer,
		VkBuildAccelerationStructureFlagsKHR flags,
		bool                                 update,
		bool                                 motion);
};


class RaytracingDescriptorSetBindings {
public:
	std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	std::vector<VkDescriptorBindingFlags> m_bindingFlags;
};


class RaytracingHandler {
	
private:
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties
		{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	RaytracingBuilder m_rtBuilder;
	DrawContext* m_models;
	
	
	RaytracingDescriptorSetBindings m_rtDescSetLayoutBind;
	VkDescriptorPool m_rtDescPool;
	VkDescriptorSetLayout m_rtDescSetLayout;
	VkDescriptorSet m_rtDescSet;

public:
	bool setup(DrawContext &drawContext);
	bool init_raytracing(VulkanEngine *engine);
	void createBottomLevelAS(VulkanEngine *engine);
	void createTopLevelAS(VulkanEngine* engine);
	void createRtDescriptorSet(VulkanEngine* engine);

	void cleanup(VkDevice device);
};




#endif // VK_RAYTRACING_H
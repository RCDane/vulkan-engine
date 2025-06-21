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
struct GlobalUniforms;

struct BlasInput {
	uint32_t id{ 0 };
	std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
	VkBuildAccelerationStructureFlagsKHR                  flags{ 0 };
};
struct ObjDesc
{
	uint64_t vertexAddress;         // Address of the Vertex buffer
	uint64_t indexAddress;          // Address of the index buffer
	uint64_t materialAddress;       // Address of the material buffer
	uint64_t materialIndexAddress;  // Address of the triangle material index buffer
	uint32_t indexOffset;
	int padding;
};

class RaytracingBuilder {
public:
	
	BlasInput objectToVkGeometry(VulkanEngine* engine, const RenderObject& object);
	void buildBlas(VulkanEngine* engine, const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags);
	VkDeviceAddress getBlasDeviceAddress(uint32_t blasId, VkDevice device);
	void createTopLevelAS(VulkanEngine* engine, const std::vector<RenderObject>& models);
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
// Push constant structure for the ray tracer
struct PushConstantRay
{
	glm::vec4  clearColor;
	glm::vec3  lightPosition;
	float lightIntensity;
	glm::vec3 lightColor;
	int  useAccumulation;
	glm::vec4 ambientColor;
	int directionalLightSamples = 1;
	int indirectLightSamples = 1;
};
enum RaytracingMode
{
	Offline,
	Realtime
};
class RaytracingHandler {
	
private:
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties
		{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	RaytracingBuilder m_rtBuilder;
	DrawContext* m_models;
	AllocatedBuffer m_rtCameraBuffer;
	AllocatedBuffer m_globalsBuffer;
	AllocatedBuffer m_bObjDesc;  // Device buffer of the OBJ descriptions
	AllocatedBuffer m_bmatDesc;  // Device buffer of the material descriptions

	std::vector<ObjDesc> objDescs;
	std::vector<MaterialConstants> materialConstants;

	VkDescriptorSet m_descSet;
	RaytracingDescriptorSetBindings m_rtDescSetLayoutBind;
	VkDescriptorSetLayout m_rtDescSetLayout;
	VkDescriptorSet m_rtDescSet;
	VkDescriptorSetLayout m_descSetLayout;

	bool lastState;

	AllocatedBuffer m_rtSBTBuffer;
	VkStridedDeviceAddressRegionKHR m_rgenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	VkStridedDeviceAddressRegionKHR m_callRegion{};


public:
	void createDescriptorSetLayout(VulkanEngine* engine);
	bool setup(VulkanEngine* engine);
	bool init_raytracing(VulkanEngine *engine);
	void prepareModelData(VulkanEngine* engine);
	void createBottomLevelAS(VulkanEngine *engine);
	void createTopLevelAS(VulkanEngine* engine);
	void createRtDescriptorSet(VulkanEngine* engine);
	void updateRtDescriptorSet(VulkanEngine* engine);
	void createRtPipeline(VulkanEngine* engine);
	void createRtShaderBindingTable(VulkanEngine* engine);
	void raytrace(VkCommandBuffer cmd, VulkanEngine* engine);
	//AllocatedBuffer _vertexIndexBuffer;

	GlobalUniforms* raytracingUniforms;
	GlobalUniforms* m_uniformMappedPtr;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
	VkPipelineLayout                                  m_rtPipelineLayout;
	VkPipeline                                        m_rtPipeline;
	PushConstantRay m_pcRay{};
	void cleanup(VkDevice device);
	int currentRayCount = 0;
	bool offlineMode = false;
	int rayBudget = 1;
};




#endif // VK_RAYTRACING_H
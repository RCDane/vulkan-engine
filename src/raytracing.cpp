#include <raytracing.h>
#include <vk_engine.h>
#include "vk_mem_alloc.h"

#include <vk_pipelines.h>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <vk_extensions.h>
#include <alignment.hpp>
#include <vk_buffers.h>


class VulkanEngine;
class DrawContext;





bool RaytracingHandler::init_raytracing(VulkanEngine *engine) {
	
	// Requesting ray tracing properties
	VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	prop2.pNext = &m_rtProperties;
	vkGetPhysicalDeviceProperties2(engine->_chosenGPU, &prop2);

	m_uniformMappedPtr = new GlobalUniforms();

	// Create the buffers during initialization
	m_globalsBuffer = create_buffer(
		&engine->_device,
		&engine->_allocator,
		sizeof(GlobalUniforms),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	


	return true;
}

uint32_t findMaxVertexIndex(const std::vector<uint32_t>& indices) {
    return *std::max_element(indices.begin(), indices.end());
}

BlasInput RaytracingBuilder::objectToVkGeometry(VulkanEngine* engine, const RenderObject& object) {
	VkDeviceAddress vertexAddress = object.vertexBufferAddress;
	VkDeviceAddress indexAddress = getBufferDeviceAddress(engine->_device, object.indexBufferRaytracing);

	uint32_t maxPrimitiveCount = object.indexCount / 3;

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	uint32_t vertexStride = sizeof(Vertex);
	triangles.vertexData.deviceAddress = vertexAddress + object.firstVertex * vertexStride;
	triangles.vertexStride = vertexStride;
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress + object.firstIndex * sizeof(uint32_t);
	triangles.maxVertex = object.maxVertex; // Correct calculation
	VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeom.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = 0;
	offset.primitiveCount = maxPrimitiveCount;
	offset.primitiveOffset = 0;
	offset.transformOffset = 0;

	BlasInput input;
	input.asGeometry.emplace_back(asGeom);
	input.asBuildOffsetInfo.emplace_back(offset);
	return input;
}

// TODO: Cleanup. This is temporarily here taken from nvvk::AccelerationStructureBuildData

// Helper function to insert a memory barrier for acceleration structures
inline void accelerationStructureBarrier(VkCommandBuffer cmd, VkAccessFlags src, VkAccessFlags dst)
{
	VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = src;
	barrier.dstAccessMask = dst;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// Convert a Mat4x4 to the matrix required by acceleration structures
inline VkTransformMatrixKHR toTransformMatrixKHR(glm::mat4 matrix)
{
	// VkTransformMatrixKHR uses a row-major memory layout, while glm::mat4
	// uses a column-major memory layout. We transpose the matrix so we can
	// memcpy the matrix's data directly.
	glm::mat4            temp = glm::transpose(matrix);
	VkTransformMatrixKHR out_matrix;
	memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
	return out_matrix;
}

// Single Geometry information, multiple can be used in a single BLAS
struct AccelerationStructureGeometryInfo
{
	VkAccelerationStructureGeometryKHR       geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
};

// Template for building Vulkan Acceleration Structures of a specified type.
struct AccelerationStructureBuildData
{
	VkAccelerationStructureTypeKHR asType = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;  // Mandatory to set

	// Collection of geometries for the acceleration structure.
	std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
	// Build range information corresponding to each geometry.
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo;
	// Build information required for acceleration structure.
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	// Size information for acceleration structure build resources.
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

	// Adds a geometry with its build range information to the acceleration structure.
	 void addGeometry(const VkAccelerationStructureGeometryKHR& asGeom, const VkAccelerationStructureBuildRangeInfoKHR& offset);
	 void addGeometry(const AccelerationStructureGeometryInfo& asGeom);

	AccelerationStructureGeometryInfo makeInstanceGeometry(size_t numInstances, VkDeviceAddress instanceBufferAddr);

	// Configures the build information and calculates the necessary size information.
	VkAccelerationStructureBuildSizesInfoKHR finalizeGeometry(VkDevice device, VkBuildAccelerationStructureFlagsKHR flags);

	// Creates an acceleration structure based on the current build and size info.
	VkAccelerationStructureCreateInfoKHR makeCreateInfo() const;

	// Commands to build the acceleration structure in a Vulkan command buffer.
	void cmdBuildAccelerationStructure(VkCommandBuffer cmd, VkAccelerationStructureKHR accelerationStructure, VkDeviceAddress scratchAddress);

	// Commands to update the acceleration structure in a Vulkan command buffer.
	void cmdUpdateAccelerationStructure(VkCommandBuffer cmd, VkAccelerationStructureKHR accelerationStructure, VkDeviceAddress scratchAddress);

	// Checks if the compact flag is set for the build.
	bool hasCompactFlag() const { return buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR; }
};
void AccelerationStructureBuildData::addGeometry(const VkAccelerationStructureGeometryKHR& asGeom,
	const VkAccelerationStructureBuildRangeInfoKHR& offset)
{
	asGeometry.push_back(asGeom);
	asBuildRangeInfo.push_back(offset);
}


void AccelerationStructureBuildData::addGeometry(const AccelerationStructureGeometryInfo& asGeom)
{
	asGeometry.push_back(asGeom.geometry);
	asBuildRangeInfo.push_back(asGeom.rangeInfo);
}


struct ScratchSizeInfo
{
	VkDeviceSize maxScratch;
	VkDeviceSize totalScratch;
};
ScratchSizeInfo calculateScratchAlignedSizes(const std::vector<AccelerationStructureBuildData>& buildData, uint32_t minAlignment)
{
	VkDeviceSize maxScratch{ 0 };
	VkDeviceSize totalScratch{ 0 };

	for (auto& buildInfo : buildData)
	{
		VkDeviceSize alignedSize = nvh::align_up(buildInfo.sizeInfo.buildScratchSize, minAlignment);
		assert(alignedSize == buildInfo.sizeInfo.buildScratchSize);  // Make sure it was already aligned
		maxScratch = std::max(maxScratch, alignedSize);
		totalScratch += alignedSize;
	}

	return { maxScratch, totalScratch };
}



class BlasBuilder
{
public:
	BlasBuilder(DescriptorAllocatorGrowable* allocator, VkDevice device);
	~BlasBuilder();

	struct Stats
	{
		VkDeviceSize totalOriginalSize = 0;
		VkDeviceSize totalCompactSize = 0;
	};


	// Create the BLAS from the vector of BlasBuildData
	// Each BLAS will be created in sequence and share the same scratch buffer
	// Return true if ALL the BLAS were created within the budget
	// if not, this function needs to be called again until it returns true
	bool cmdCreateBlas(VkCommandBuffer                              cmd,
		std::vector<AccelerationStructureBuildData>& blasBuildData,   // List of the BLAS to build */
		std::vector<AccelKHR>& blasAccel,       // List of the acceleration structure
		VkDeviceAddress                              scratchAddress,  //  Address of the scratch buffer
		VkDeviceSize                                 hintMaxBudget = 512'000'000);

	// Create the BLAS from the vector of BlasBuildData in parallel
	// The advantage of this function is that it will try to build as many BLAS as possible in parallel
	// but it requires a scratch buffer per BLAS, or less but then each of them must large enough to hold the largest BLAS
	// This function needs to be called until it returns true
	bool cmdCreateParallelBlas(VkCommandBuffer                              cmd,
		std::vector<AccelerationStructureBuildData>& blasBuildData,
		std::vector<AccelKHR>& blasAccel,
		const std::vector<VkDeviceAddress>& scratchAddress,
		VkDeviceSize                                 hintMaxBudget = 512'000'000);

	// Compact the BLAS that have been built
	// Synchronization must be done by the application between the build and the compact
	void cmdCompactBlas(VkCommandBuffer cmd, std::vector<AccelerationStructureBuildData>& blasBuildData, std::vector<AccelKHR>& blasAccel);

	// Destroy the original BLAS that was compacted
	void destroyNonCompactedBlas();

	// Destroy all information
	void destroy();

	// Return the statistics about the compacted BLAS
	Stats getStatistics() const { return m_stats; };

	// Scratch size strategy:
	// Find the maximum size of the scratch buffer needed for the BLAS build
	//
	// Strategy:
	// - Loop over all BLAS to find the maximum size
	// - If the max size is within the budget, return it. This will return as many addresses as there are BLAS
	// - Otherwise, return n*maxBlasSize, where n is the number of BLAS and maxBlasSize is the maximum size found for a single BLAS.
	//   In this case, fewer addresses will be returned than the number of BLAS, but each can be used to build any BLAS
	//
	// Usage
	// - Call this function to get the maximum size needed for the scratch buffer
	// - User allocate the scratch buffer with the size returned by this function
	// - Call getScratchAddresses to get the address for each BLAS
	//
	// Note: 128 is the default alignment for the scratch buffer
	//       (VkPhysicalDeviceAccelerationStructurePropertiesKHR::minAccelerationStructureScratchOffsetAlignment)
	VkDeviceSize getScratchSize(VkDeviceSize                                             hintMaxBudget,
		const std::vector<AccelerationStructureBuildData>& buildData,
		uint32_t                                                 minAlignment = 128) const;

	void getScratchAddresses(VkDeviceSize                                             hintMaxBudget,
		const std::vector<AccelerationStructureBuildData>& buildData,
		VkDeviceAddress                                          scratchBufferAddress,
		std::vector<VkDeviceAddress>& scratchAddresses,
		uint32_t                                                 minAlignment = 128);


private:
	void         destroyQueryPool();
	void         createQueryPool(uint32_t maxBlasCount);
	void         initializeQueryPoolIfNeeded(const std::vector<AccelerationStructureBuildData>& blasBuildData);
	VkDeviceSize buildAccelerationStructures(VkCommandBuffer                              cmd,
		std::vector<AccelerationStructureBuildData>& blasBuildData,
		std::vector<AccelKHR>& blasAccel,
		const std::vector<VkDeviceAddress>& scratchAddress,
		VkDeviceSize                                 hintMaxBudget,
		VkDeviceSize                                 currentBudget,
		uint32_t& currentQueryIdx);

	VkDevice                 m_device;
	DescriptorAllocatorGrowable* m_alloc = nullptr;
	VkQueryPool              m_queryPool = VK_NULL_HANDLE;
	uint32_t                 m_currentBlasIdx{ 0 };
	uint32_t                 m_currentQueryIdx{ 0 };

	std::vector<AccelKHR> m_cleanupBlasAccel;

	// Stats
	Stats m_stats;
};

VkAccelerationStructureCreateInfoKHR AccelerationStructureBuildData::makeCreateInfo() const
{
	assert(asType != VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR && "Acceleration Structure Type not set");
	assert(sizeInfo.accelerationStructureSize > 0 && "Acceleration Structure Size not set");

	VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = asType;
	createInfo.size = sizeInfo.accelerationStructureSize;

	return createInfo;
}


// Initializes a query pool for recording acceleration structure properties if necessary.
// This function ensures a query pool is available if any BLAS in the build data is flagged for compaction.
void BlasBuilder::initializeQueryPoolIfNeeded(const std::vector<AccelerationStructureBuildData>& blasBuildData)
{
	if (!m_queryPool)
	{
		// Iterate through each BLAS build data element to check if the compaction flag is set.
		for (const auto& blas : blasBuildData)
		{
			if (blas.hasCompactFlag())
			{
				createQueryPool(static_cast<uint32_t>(blasBuildData.size()));
				break;
			}
		}
	}

	// If a query pool is now available (either newly created or previously existing),
	// reset the query pool to clear any old data or states.
	if (m_queryPool)
	{
		vkResetQueryPool(m_device, m_queryPool, 0, static_cast<uint32_t>(blasBuildData.size()));
	}
}


// Builds multiple Bottom-Level Acceleration Structures (BLAS) for a Vulkan ray tracing pipeline.
// This function manages memory budgets and submits the necessary commands to the specified command buffer.
//
// Parameters:
//   cmd            - Command buffer where acceleration structure commands are recorded.
//   blasBuildData  - Vector of data structures containing the geometry and other build-related information for each BLAS.
//   blasAccel      - Vector where the function will store the created acceleration structures.
//   scratchAddress - Vector of device addresses pointing to scratch memory required for the build process.
//   hintMaxBudget  - A hint for the maximum budget allowed for building acceleration structures.
//   currentBudget  - The current usage of the budget prior to this call.
//   currentQueryIdx - Reference to the current index for queries, updated during execution.
//
// Returns:
//   The total device size used for building the acceleration structures during this function call.
VkDeviceSize BlasBuilder::buildAccelerationStructures(VkCommandBuffer                              cmd,
	std::vector<AccelerationStructureBuildData>& blasBuildData,
	std::vector<AccelKHR>& blasAccel,
	const std::vector<VkDeviceAddress>& scratchAddress,
	VkDeviceSize                                 hintMaxBudget,
	VkDeviceSize                                 currentBudget,
	uint32_t& currentQueryIdx)
{
	// Temporary vectors for storing build-related data
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> collectedBuildInfo;
	std::vector<VkAccelerationStructureKHR>                  collectedAccel;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*>   collectedRangeInfo;

	// Pre-allocate memory based on the number of BLAS to be built
	collectedBuildInfo.reserve(blasBuildData.size());
	collectedAccel.reserve(blasBuildData.size());
	collectedRangeInfo.reserve(blasBuildData.size());

	// Initialize the total budget used in this function call
	VkDeviceSize budgetUsed = 0;

	// Loop through BLAS data while there is scratch address space and budget available
	while (collectedBuildInfo.size() < scratchAddress.size() && currentBudget + budgetUsed < hintMaxBudget
		&& m_currentBlasIdx < blasBuildData.size())
	{
		auto& data = blasBuildData[m_currentBlasIdx];
		VkAccelerationStructureCreateInfoKHR createInfo = data.makeCreateInfo();

		// Create and store acceleration structure
		blasAccel[m_currentBlasIdx] = m_alloc->createAcceleration(m_device,createInfo);
		collectedAccel.push_back(blasAccel[m_currentBlasIdx].accel);

		// Setup build information for the current BLAS
		data.buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		data.buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		data.buildInfo.dstAccelerationStructure = blasAccel[m_currentBlasIdx].accel;
		data.buildInfo.scratchData.deviceAddress = scratchAddress[m_currentBlasIdx];
		data.buildInfo.pGeometries = data.asGeometry.data();
		collectedBuildInfo.push_back(data.buildInfo);
		collectedRangeInfo.push_back(data.asBuildRangeInfo.data());

		// Update the used budget with the size of the current structure
		budgetUsed += data.sizeInfo.accelerationStructureSize;
		m_currentBlasIdx++;
	}

	// Command to build the acceleration structures on the GPU
	vkCmdBuildAccelerationStructuresKHR(cmd, static_cast<uint32_t>(collectedBuildInfo.size()), collectedBuildInfo.data(),
		collectedRangeInfo.data());

	// Barrier to ensure proper synchronization after building
	accelerationStructureBarrier(cmd, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

	// If a query pool is available, record the properties of the built acceleration structures
	if (m_queryPool)
	{
		vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, static_cast<uint32_t>(collectedAccel.size()), collectedAccel.data(),
			VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, m_queryPool,
			currentQueryIdx);
		currentQueryIdx += static_cast<uint32_t>(collectedAccel.size());
	}

	// Return the total budget used in this operation
	return budgetUsed;
}


void BlasBuilder::createQueryPool(uint32_t maxBlasCount)
{
	VkQueryPoolCreateInfo qpci = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
	qpci.queryCount = maxBlasCount;
	vkCreateQueryPool(m_device, &qpci, nullptr, &m_queryPool);
}

// This will build multiple BLAS serially, one after the other, ensuring that the process
// stays within the specified memory budget.
bool BlasBuilder::cmdCreateBlas(VkCommandBuffer                              cmd,
	std::vector<AccelerationStructureBuildData>& blasBuildData,
	std::vector<AccelKHR>& blasAccel,
	VkDeviceAddress                              scratchAddress,
	VkDeviceSize                                 hintMaxBudget)
{
	// It won't run in parallel, but will process all BLAS within the budget before returning
	return cmdCreateParallelBlas(cmd, blasBuildData, blasAccel, { scratchAddress }, hintMaxBudget);
}

// This function is responsible for building multiple Bottom-Level Acceleration Structures (BLAS) in parallel,
// ensuring that the process stays within the specified memory budget.
//
// Returns:
//   A boolean indicating whether all BLAS in the `blasBuildData` have been built by this function call.
//   Returns `true` if all BLAS were built, `false` otherwise.
bool BlasBuilder::cmdCreateParallelBlas(VkCommandBuffer                              cmd,
	std::vector<AccelerationStructureBuildData>& blasBuildData,
	std::vector<AccelKHR>& blasAccel,
	const std::vector<VkDeviceAddress>& scratchAddress,
	VkDeviceSize                                 hintMaxBudget)
{
	// Initialize the query pool if necessary to handle queries for properties of built acceleration structures.
	initializeQueryPoolIfNeeded(blasBuildData);

	VkDeviceSize processBudget = 0;                  // Tracks the total memory used in the construction process.
	uint32_t     currentQueryIdx = m_currentQueryIdx;  // Local copy of the current query index.

	// Process each BLAS in the data vector while staying under the memory budget.
	while (m_currentBlasIdx < blasBuildData.size() && processBudget < hintMaxBudget)
	{
		// Build acceleration structures and accumulate the total memory used.
		processBudget += buildAccelerationStructures(cmd, blasBuildData, blasAccel, scratchAddress, hintMaxBudget,
			processBudget, currentQueryIdx);
	}

	// Check if all BLAS have been built.
	return m_currentBlasIdx >= blasBuildData.size();
}

// Compacts the Bottom-Level Acceleration Structures (BLAS) that have been built, reducing their memory footprint.
// This function uses the results from previously performed queries to determine the compacted sizes and then
// creates new, smaller acceleration structures. It also handles copying from the original to the compacted structures.
//
// Notes:
//   It assumes that a query has been performed earlier to determine the possible compacted sizes of the acceleration structures.
//
void BlasBuilder::cmdCompactBlas(VkCommandBuffer                                    cmd,
	std::vector<AccelerationStructureBuildData>& blasBuildData,
	std::vector<AccelKHR>& blasAccel)
{
	// Compute the number of queries that have been conducted between the current BLAS index and the query index.
	uint32_t queryCtn = m_currentBlasIdx - m_currentQueryIdx;
	// Ensure there is a valid query pool and BLAS to compact;
	if (m_queryPool == VK_NULL_HANDLE || queryCtn == 0)
	{
		return;
	}


	// Retrieve the compacted sizes from the query pool.
	std::vector<VkDeviceSize> compactSizes(queryCtn);
	vkGetQueryPoolResults(m_device, m_queryPool, m_currentQueryIdx, (uint32_t)compactSizes.size(),
		compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize),
		VK_QUERY_RESULT_WAIT_BIT);

	// Iterate through each BLAS index to process compaction.
	for (size_t i = m_currentQueryIdx; i < m_currentBlasIdx; i++)
	{
		size_t       idx = i - m_currentQueryIdx;  // Calculate local index for compactSizes vector.
		VkDeviceSize compactSize = compactSizes[idx];
		if (compactSize > 0)
		{
			// Update statistical tracking of sizes before and after compaction.
			m_stats.totalCompactSize += compactSize;
			m_stats.totalOriginalSize += blasBuildData[i].sizeInfo.accelerationStructureSize;
			blasBuildData[i].sizeInfo.accelerationStructureSize = compactSize;
			m_cleanupBlasAccel.push_back(blasAccel[i]);  // Schedule old BLAS for cleanup.

			// Create a new acceleration structure for the compacted BLAS.
			VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			asCreateInfo.size = compactSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			blasAccel[i] = m_alloc->createAcceleration(m_device,asCreateInfo);

			// Command to copy the original BLAS to the newly created compacted version.
			VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
			copyInfo.src = blasBuildData[i].buildInfo.dstAccelerationStructure;
			copyInfo.dst = blasAccel[i].accel;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

			// Update the build data to reflect the new destination of the BLAS.
			blasBuildData[i].buildInfo.dstAccelerationStructure = blasAccel[i].accel;
		}
	}

	// Update the query index to the current BLAS index, marking the end of processing for these structures.
	m_currentQueryIdx = m_currentBlasIdx;
}


void BlasBuilder::destroyNonCompactedBlas()
{
	for (auto& blas : m_cleanupBlasAccel)
	{
		blas.destroy(m_device);
	}
	m_cleanupBlasAccel.clear();
}


// Find if the total scratch size is within the budget, otherwise return n-time the max scratch size that fits in the budget
VkDeviceSize BlasBuilder::getScratchSize(VkDeviceSize                                             hintMaxBudget,
	const std::vector<AccelerationStructureBuildData>& buildData,
	uint32_t minAlignment /*= 128*/) const
{
	ScratchSizeInfo sizeInfo = calculateScratchAlignedSizes(buildData, minAlignment);
	VkDeviceSize    maxScratch = sizeInfo.maxScratch;
	VkDeviceSize    totalScratch = sizeInfo.totalScratch;

	if (totalScratch < hintMaxBudget)
	{
		return totalScratch;
	}
	else
	{
		uint64_t numScratch = std::max(uint64_t(1), hintMaxBudget / maxScratch);
		numScratch = std::min((size_t)numScratch, buildData.size());
		return numScratch * maxScratch;
	}
}

BlasBuilder::BlasBuilder(DescriptorAllocatorGrowable* allocator, VkDevice device)
	: m_device(device)
	, m_alloc(allocator)
{
}



void BlasBuilder::destroyQueryPool()
{
	if (m_queryPool)
	{
		vkDestroyQueryPool(m_device, m_queryPool, nullptr);
		m_queryPool = VK_NULL_HANDLE;
	}
}


void BlasBuilder::destroy()
{
	destroyQueryPool();
	destroyNonCompactedBlas();
}

BlasBuilder::~BlasBuilder()
{
	destroy();
}

VkAccelerationStructureBuildSizesInfoKHR AccelerationStructureBuildData::finalizeGeometry(VkDevice device, VkBuildAccelerationStructureFlagsKHR flags)
{
	assert(device != VK_NULL_HANDLE && "VkDevice is null");
	assert(asGeometry.size() > 0 && "No geometry added to Build Structure");
	assert(asType != VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR && "Acceleration Structure Type not set");

	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = asType;
	buildInfo.flags = flags;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.geometryCount = static_cast<uint32_t>(asGeometry.size());
	buildInfo.pGeometries = asGeometry.data();
	buildInfo.ppGeometries = nullptr;
	buildInfo.scratchData.deviceAddress = 0;

	assert(buildInfo.geometryCount > 0 && "Geometry count is zero");
	assert(buildInfo.pGeometries != nullptr && "Geometry data is null");

	std::vector<uint32_t> maxPrimCount(asBuildRangeInfo.size());
	for (size_t i = 0; i < asBuildRangeInfo.size(); ++i)
	{
		maxPrimCount[i] = asBuildRangeInfo[i].primitiveCount;
	}

	vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
		maxPrimCount.data(), &sizeInfo);

	return sizeInfo;
}

// Return the scratch addresses fitting the scrath strategy (see above)
void BlasBuilder::getScratchAddresses(VkDeviceSize hintMaxBudget,
	const std::vector<AccelerationStructureBuildData>& buildData,
	VkDeviceAddress scratchBufferAddress,
	std::vector<VkDeviceAddress>& scratchAddresses,
	uint32_t minAlignment /*=128*/)
{
	// 1. Manually align the incoming base to a multiple of minAlignment
	VkDeviceAddress alignedBase = (scratchBufferAddress + (minAlignment - 1)) & ~(minAlignment - 1);

	// 2. Compute how big the total/maximum scratch usage is
	ScratchSizeInfo sizeInfo = calculateScratchAlignedSizes(buildData, minAlignment);
	VkDeviceSize    maxScratch = sizeInfo.maxScratch;
	VkDeviceSize    totalScratch = sizeInfo.totalScratch;

	// 3. Decide which strategy you're using (the old code had 2 "paths")
	if (totalScratch < hintMaxBudget)
	{
		// Strategy 1: We can provide a unique scratch region for each BLAS
		VkDeviceSize offset = 0; // offset from alignedBase
		for (auto& b : buildData)
		{
			// The device address for this BLAS
			VkDeviceAddress subAddr = alignedBase + offset;
			scratchAddresses.push_back(subAddr);

			// Advance offset by the BLAS�s required scratch size (rounded up)
			VkDeviceSize alignedSize = nvh::align_up(b.sizeInfo.buildScratchSize, minAlignment);
			offset += alignedSize;
		}
	}
	else
	{
		// Strategy 2: We only have enough memory to do 'N' BLAS in parallel
		//  (n-times the largest scratch)
		uint64_t numScratch = std::max(uint64_t(1), hintMaxBudget / maxScratch);
		numScratch = std::min((size_t)numScratch, buildData.size());

		VkDeviceSize offset = 0;
		for (int i = 0; i < static_cast<int>(numScratch); i++)
		{
			VkDeviceAddress subAddr = alignedBase + offset;
			scratchAddresses.push_back(subAddr);
			offset += maxScratch;
			// Here you might also want to do offset = nvh::align_up(offset, minAlignment)
			// but since maxScratch should already be a multiple of minAlignment, that�s typically OK
		}
	}
}



void RaytracingBuilder::buildBlas(VulkanEngine* engine, const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags) {
	auto numBlas = static_cast<uint32_t>(input.size());
	std::vector<AccelerationStructureBuildData> blasBuildData(numBlas);
	m_blas.resize(numBlas);

	for (uint32_t idx = 0; idx < numBlas; idx++) {
		blasBuildData[idx].asType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		blasBuildData[idx].asGeometry = input[idx].asGeometry;
		blasBuildData[idx].asBuildRangeInfo = input[idx].asBuildOffsetInfo;

		blasBuildData[idx].finalizeGeometry(engine->_device, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	}

	VkDeviceSize hintMaxBudget{ 300'000'0000 };  // 256 MB
	bool hasCompaction = hasFlag(flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	BlasBuilder blasBuilder(&engine->globalDescriptorAllocator, engine->_device);

	uint32_t minAlignment = 128; // Typically from device properties

	// Instead of a single scratch buffer, create multiple
	std::vector<AllocatedBuffer> scratchBuffers;
	std::vector<VkDeviceAddress> scratchAddresses;

	for (uint32_t i = 0; i < numBlas; ++i) {
		VkDeviceSize scratchSize = blasBuildData[i].sizeInfo.buildScratchSize;
		VkDeviceSize alignedSize = nvh::align_up(scratchSize, minAlignment);

		AllocatedBuffer scratchBuffer = create_buffer(
			&engine->_device,
			&engine->_allocator,
			alignedSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, // Prefer GPU-only memory
			VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT
		);

		VkDeviceAddress address = getBufferDeviceAddress(engine->_device, scratchBuffer.buffer);
		assert((address % minAlignment) == 0 && "Scratch buffer address is not properly aligned");
		scratchAddresses.push_back(address);
		scratchBuffers.push_back(scratchBuffer);
	}

	// Now, associate each BLAS build with its own scratch buffer
	engine->immediate_submit([&](VkCommandBuffer cmd) {
		blasBuilder.cmdCreateParallelBlas(cmd, blasBuildData, m_blas, scratchAddresses, hintMaxBudget);
		});

	if (hasCompaction) {
		engine->immediate_submit([&](VkCommandBuffer cmd) {
			blasBuilder.cmdCreateParallelBlas(cmd, blasBuildData, m_blas, scratchAddresses, hintMaxBudget);
			blasBuilder.cmdCompactBlas(cmd, blasBuildData, m_blas);
			});

		blasBuilder.destroyNonCompactedBlas();
	}

	// Cleanup all scratch buffers after ensuring GPU has finished using them
	for (auto& buffer : scratchBuffers) {
		destroy_buffer(buffer);
	}
}

bool  RaytracingHandler::setup(VulkanEngine* engine) {

	m_models = &engine->mainDrawContext;
	

	
	return true;
}

glm::uvec2 pack_uint_64(uint64_t value)
{
	return glm::uvec2(value & 0xFFFFFFFF, value >> 32);
}
void printAddressInHex(VkDeviceAddress address) {
	std::cout << "Address: 0x" << std::hex << std::setw(16) << std::setfill('0') << address << std::dec << std::endl;
}
ObjDesc prepareModel(VulkanEngine* engine, RenderObject renderObject) {
	ObjDesc objModel;
	VkDeviceAddress indexAddress = getBufferDeviceAddress(engine->_device, renderObject.indexBuffer);

	VkDeviceAddress vertexAddress = renderObject.vertexBufferAddress;


	indexAddress = indexAddress + renderObject.firstIndex * sizeof(int);

	objModel.vertexAddress = vertexAddress;
	objModel.indexAddress = indexAddress;
	objModel.indexOffset = renderObject.firstIndex;


	return objModel;
}


void RaytracingHandler::prepareModelData(VulkanEngine* engine) {
	materialConstants.reserve(m_models->OpaqueSurfaces.size());
	for (auto& obj : m_models->OpaqueSurfaces) {
		objDescs.emplace_back(prepareModel(engine,obj));
		materialConstants.push_back(*obj.material->data);
	}

	m_bmatDesc = create_buffer(
		&engine->_device,
		&engine->_allocator,
		materialConstants.size() * sizeof(MaterialConstants),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	
	
	m_bObjDesc = create_buffer(
		&engine->_device,
		&engine->_allocator,
		objDescs.size()*sizeof(ObjDesc),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);



	// Map material constants
	void* mappedData = nullptr;
	vmaMapMemory(engine->_allocator, m_bmatDesc.allocation, &mappedData);
	if (mappedData) {
		// Copy ObjDesc data into the mapped buffer
		memcpy(mappedData, materialConstants.data(), materialConstants.size() * sizeof(MaterialConstants));
		vmaUnmapMemory(engine->_allocator, m_bmatDesc.allocation);
	}
	else {
		throw std::runtime_error("Failed to map scratch buffer for material data.");
	}

	// map object descriptions
	mappedData = nullptr;
	vmaMapMemory(engine->_allocator, m_bObjDesc.allocation, &mappedData);
	if (mappedData) {
		// Copy ObjDesc data into the mapped buffer
		memcpy(mappedData, objDescs.data(), objDescs.size() * sizeof(ObjDesc));
		vmaUnmapMemory(engine->_allocator, m_bObjDesc.allocation);
	}
	else {
		throw std::runtime_error("Failed to map scratch buffer for ObjDesc data.");
	}
	
}

void RaytracingHandler::createBottomLevelAS(VulkanEngine* engine)
{
	// BLAS - Storing each primitive in a geometry
	std::vector<BlasInput> allBlas;
	allBlas.reserve(m_models->OpaqueSurfaces.size()); 
	for (const auto& obj : m_models->OpaqueSurfaces)
	{
		auto blas = m_rtBuilder.objectToVkGeometry(engine,obj);

		// We could add more geometry in each BLAS, but we add only one for now
		allBlas.emplace_back(blas);
	}
	m_rtBuilder.buildBlas(engine,allBlas,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR ||
		VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR
	);
}



VkDeviceAddress RaytracingBuilder::getBlasDeviceAddress(uint32_t blasId, VkDevice device)
{
	assert(size_t(blasId) < m_blas.size());
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	addressInfo.accelerationStructure = m_blas[blasId].accel;
	return vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}


AccelerationStructureGeometryInfo AccelerationStructureBuildData::makeInstanceGeometry(size_t numInstances,
	VkDeviceAddress instanceBufferAddr)
{
	assert(asType == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR && "Instance geometry can only be used with TLAS");

	// Describes instance data in the acceleration structure.
	VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	geometryInstances.data.deviceAddress = instanceBufferAddr;

	// Set up the geometry to use instance data.
	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = geometryInstances;

	// Specifies the number of primitives (instances in this case).
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
	rangeInfo.primitiveCount = static_cast<uint32_t>(numInstances);

	// Prepare and return geometry information.
	AccelerationStructureGeometryInfo result;
	result.geometry = geometry;
	result.rangeInfo = rangeInfo;

	return result;
}

void AccelerationStructureBuildData::cmdBuildAccelerationStructure(VkCommandBuffer cmd,
	VkAccelerationStructureKHR accelerationStructure,
	VkDeviceAddress scratchAddress)
{
	assert(asGeometry.size() == asBuildRangeInfo.size() && "asGeometry.size() != asBuildRangeInfo.size()");
	assert(accelerationStructure != VK_NULL_HANDLE && "Acceleration Structure not created, first call createAccelerationStructure");

	const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo = asBuildRangeInfo.data();

	// Build the acceleration structure
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	buildInfo.pGeometries = asGeometry.data();  // In case the structure was copied, we need to update the pointer

	vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangeInfo);

	// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
	// is finished before starting the next one.
	accelerationStructureBarrier(cmd, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
}


void AccelerationStructureBuildData::cmdUpdateAccelerationStructure(VkCommandBuffer cmd,
	VkAccelerationStructureKHR accelerationStructure,
	VkDeviceAddress scratchAddress)
{
	assert(asGeometry.size() == asBuildRangeInfo.size() && "asGeometry.size() != asBuildRangeInfo.size()");
	assert(accelerationStructure != VK_NULL_HANDLE && "Acceleration Structure not created, first call createAccelerationStructure");

	const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo = asBuildRangeInfo.data();

	// Build the acceleration structure
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	buildInfo.srcAccelerationStructure = accelerationStructure;
	buildInfo.dstAccelerationStructure = accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	buildInfo.pGeometries = asGeometry.data();
	vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangeInfo);

	// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
	// is finished before starting the next one.
	accelerationStructureBarrier(cmd, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
}



void RaytracingBuilder::cmdCreateTlas(
	VkCommandBuffer cmdBuf,
	VulkanEngine* engine,
	uint32_t countInstance,
	VkDeviceAddress instBufferAddr,
	AllocatedBuffer& scratchBuffer,
	VkBuildAccelerationStructureFlagsKHR flags,
	bool update,
	bool motion)
{
	AccelerationStructureBuildData tlasBuildData;
	tlasBuildData.asType = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

	AccelerationStructureGeometryInfo geo = tlasBuildData.makeInstanceGeometry(countInstance, instBufferAddr);
	tlasBuildData.addGeometry(geo);

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = tlasBuildData.finalizeGeometry(engine->_device, flags);

	// Allocate the scratch memory with proper alignment
	static const VkDeviceSize kScratchAlign = 128; // Typically from device AS properties
	VkDeviceSize scratchSize = update ? sizeInfo.updateScratchSize : sizeInfo.buildScratchSize;
	VkDeviceSize alignedScratchSize = nvh::align_up(scratchSize, kScratchAlign);

	scratchBuffer = create_buffer(
		&engine->_device,
		&engine->_allocator,
		alignedScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_CPU_TO_GPU, // Or GPU_TO_CPU if necessary
		VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT
	);

	VkDeviceAddress scratchAddress = getBufferDeviceAddress(engine->_device, scratchBuffer.buffer);
	VkDeviceAddress alignedScratchAddr = (scratchAddress + (kScratchAlign - 1)) & ~(kScratchAlign - 1);

	if (update)
	{
		tlasBuildData.asGeometry[0].geometry.instances.data.deviceAddress = instBufferAddr;
		tlasBuildData.cmdUpdateAccelerationStructure(cmdBuf, m_tlas.accel, scratchAddress);
	}
	else
	{
		VkAccelerationStructureCreateInfoKHR createInfo = tlasBuildData.makeCreateInfo();
		m_tlas = engine->globalDescriptorAllocator.createAcceleration(engine->_device, createInfo);
		tlasBuildData.cmdBuildAccelerationStructure(cmdBuf, m_tlas.accel, scratchAddress);
	}
}



// Build TLAS from an array of VkAccelerationStructureInstanceKHR
  // - Use motion=true with VkAccelerationStructureMotionInstanceNV
  // - The resulting TLAS will be stored in m_tlas
  // - update is to rebuild the Tlas with updated matrices, flag must have the 'allow_update'
void RaytracingBuilder::buildTlas(
	VkCommandBuffer cmd,
	VulkanEngine* engine,
	const std::vector<VkAccelerationStructureInstanceKHR>& instances,
	VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
	bool update = false,
	bool motion = false)
{
	// Ensure TLAS is either not built yet or being updated
	assert(m_tlas.accel == VK_NULL_HANDLE || update);
	uint32_t countInstance = static_cast<uint32_t>(instances.size());

	// Step 1: Allocate the instance buffer with CPU-to-GPU memory
	AllocatedBuffer instancesBuffer = create_buffer(
		&engine->_device,
		&engine->_allocator,
		countInstance * sizeof(VkAccelerationStructureInstanceKHR),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_CPU_TO_GPU, // Changed from GPU_ONLY to CPU_TO_GPU
		VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT
	);

	// Step 2: Map the buffer and copy instance data
	void* data;
	VkResult mapResult = vmaMapMemory(engine->_allocator, instancesBuffer.allocation, &data);
	if (mapResult != VK_SUCCESS) {
		throw std::runtime_error("Failed to map instance buffer memory!");
	}
	memcpy(data, instances.data(), countInstance * sizeof(VkAccelerationStructureInstanceKHR));
	vmaUnmapMemory(engine->_allocator, instancesBuffer.allocation);

	// Optionally, flush the memory to ensure visibility (handled by VMA if necessary)
	// vmaFlushAllocation(engine->_allocator, instancesBuffer.allocation, 0, VK_WHOLE_SIZE);

	// Step 3: Get the device address of the populated instance buffer
	VkBufferDeviceAddressInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferInfo.buffer = instancesBuffer.buffer;
	VkDeviceAddress instBufferAddr = vkGetBufferDeviceAddress(engine->_device, &bufferInfo);

	// Step 4: Insert a memory barrier to ensure the instance data is available before TLAS build
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; // Since we wrote from the CPU
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0,
		1, &barrier,
		0, nullptr,
		0, nullptr
	);

	// Step 5: Create and build the TLAS
	AllocatedBuffer scratchBuffer;
	cmdCreateTlas(
		cmd,
		engine,
		countInstance,
		instBufferAddr,
		scratchBuffer,
		flags,
		update,
		motion
	);

	// Step 6: Clean up the scratch buffer if it's temporary
	// (Assuming cmdCreateTlas manages its own scratch buffer; adjust as needed)
}




void RaytracingBuilder::createTopLevelAS(VulkanEngine* engine, const std::vector<RenderObject>& models) {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	tlas.reserve(models.size());
	uint32_t index = 0;
	for (const auto& inst : models)
	{
		VkAccelerationStructureInstanceKHR rayInst = {};
		rayInst.transform = toTransformMatrixKHR(inst.transform);  // Position of the instance
		rayInst.instanceCustomIndex = index;            // Custom index
		rayInst.accelerationStructureReference = getBlasDeviceAddress(index, engine->_device);
		rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rayInst.mask = 0xFF; // Only be hit if rayMask & instance.mask != 0
		rayInst.instanceShaderBindingTableRecordOffset = 0; // Hit group index
		tlas.emplace_back(rayInst);
		index++;
	}

	engine->immediate_submit([&](VkCommandBuffer cmd) {
		buildTlas(
			cmd,
			engine,
			tlas,
			VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		);
		});
}


void RaytracingHandler::createTopLevelAS(VulkanEngine* engine)
{
	m_rtBuilder.createTopLevelAS(engine, m_models->OpaqueSurfaces);
}

void RaytracingBuilder::destroy(VkDevice device) {
	
	
	for (auto& b : m_blas)
	{
		b.destroy(device);
	}
	m_tlas.destroy(device);
	
	m_blas.clear();
	m_blas = {};
}


void RaytracingHandler::cleanup(VkDevice device) {
	m_rtBuilder.destroy(device);
	vkDestroyPipeline(device, m_rtPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_rtPipelineLayout, nullptr);
	
}

// TODO: This function needs to be cleaned up. Is probably not necessary.
void RaytracingHandler::createDescriptorSetLayout(VulkanEngine* engine) {
	DescriptorLayoutBuilder builder;

	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;




	builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, flags);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, flags, 300);
	builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, flags, 300);
	builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,flags, 300);
	builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, flags, 1);
	//builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, flags, 100);
	VkDescriptorSetLayoutCreateFlags createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	m_descSetLayout = builder.build(engine->_device,
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_RAYGEN_BIT_KHR |
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
		VK_SHADER_STAGE_MISS_BIT_KHR,
		NULL, createFlags);

	m_descSet = engine->updatingGlobalDescriptorAllocator.allocate(engine->_device, m_descSetLayout);

}


void RaytracingHandler::createRtDescriptorSet(VulkanEngine *engine) {
	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	
	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags);

	VkDescriptorSetLayoutCreateFlags createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

	m_rtDescSetLayout = builder.build(engine->_device,
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		NULL,
		createFlags);
	m_rtDescSet = engine->updatingGlobalDescriptorAllocator.allocate(engine->_device, m_rtDescSetLayout);
	
	VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
	
	VkWriteDescriptorSetAccelerationStructureKHR asInfo = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	asInfo.accelerationStructureCount = 1;
	asInfo.pAccelerationStructures = &tlas;

	DescriptorWriter writer;
	writer.write_acceleration_structure(0, asInfo);
	writer.write_image(1, 
		engine->_drawImage.imageView, 
		VK_NULL_HANDLE  // Need to define sampler
		,VK_IMAGE_LAYOUT_GENERAL, 
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(engine->_device, m_rtDescSet);
}

void RaytracingHandler::updateRtDescriptorSet(VulkanEngine* engine) {
	VkDescriptorImageInfo imageInfo{ {}, engine->_drawImage.imageView, VK_IMAGE_LAYOUT_GENERAL };
	DescriptorWriter writer;
	writer.write_image(
		1, 
		engine->_drawImage.imageView, 
		VK_NULL_HANDLE, 
		VK_IMAGE_LAYOUT_GENERAL, 
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(engine->_device, m_rtDescSet);
}

void RaytracingHandler::createRtPipeline(VulkanEngine* engine) {
	enum StageIndices {
		eRaygen, eMiss, eMiss2, eClosestHit, eShaderGroupCount
	};

	// All stages
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.pName = "main";  // All the same entry point

	// Raygen
	VkShaderModule main;
	if (!vkutil::load_shader_module("../shaders/spv/raytraceAccIter.rgen.spv", engine->_device, &main)) {
		fmt::print("Error when building the raygen shader module\n");
	}
	else {
		fmt::print("Raygen shader successfully loaded\n");
	}
	stage.module = main;
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[eRaygen] = stage;

	// Miss
	VkShaderModule miss;
	if (!vkutil::load_shader_module("../shaders/spv/raytrace.rmiss.spv", engine->_device, &miss)) {
		fmt::print("Error when building the miss shader module\n");
	}
	else {
		fmt::print("Miss shader successfully loaded\n");
	}
	stage.module = miss;
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss] = stage;

	// The second miss shader is invoked when a shadow ray misses the geometry. It simply indicates that no occlusion has been found
	VkShaderModule miss2;
	if (!vkutil::load_shader_module("../shaders/spv/raytraceShadow.rmiss.spv", engine->_device, &miss2)) {
		fmt::print("Error when building the shadow miss shader module\n");
	}
	else {
		fmt::print("Shadow miss shader successfully loaded\n");
	}
	stage.module = miss2;
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss2] = stage;

	// Hit Group - Closest Hit
	VkShaderModule closestHit;
	if (!vkutil::load_shader_module("../shaders/spv/raytracepbrAccIter.rchit.spv", engine->_device, &closestHit)) {
		fmt::print("Error when building the closest hit shader module\n");
	}
	else {
		fmt::print("Closest hit shader successfully loaded\n");
	}
	stage.module = closestHit;
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[eClosestHit] = stage;

	// Shader groups
	VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;

	// Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eRaygen;
	m_rtShaderGroups.push_back(group);

	// Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss;
	m_rtShaderGroups.push_back(group);

	// Shadow Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss2;
	m_rtShaderGroups.push_back(group);

	// Closest hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	m_rtShaderGroups.push_back(group);

	// Push constant: we want to be able to update constants used by the shaders
	VkPushConstantRange pushConstant{ VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
									  0, sizeof(PushConstantRay) };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

	// Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
	std::vector<VkDescriptorSetLayout> rtDescSetLayouts = { m_rtDescSetLayout, m_descSetLayout, engine->_deferredDscSetLayout, engine->_lightsourceDescriptorSetLayout };
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();
	
	vkCreatePipelineLayout(engine->_device, &pipelineLayoutCreateInfo, nullptr, &m_rtPipelineLayout);

	// Assemble the shader stages and recursion depth info into the ray tracing pipeline
	VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  // Stages are shaders
	rayPipelineInfo.pStages = stages.data();

	// In this case, m_rtShaderGroups.size() == 3: we have one raygen group,
	// one miss shader group, and one hit group.
	rayPipelineInfo.groupCount = static_cast<uint32_t>(m_rtShaderGroups.size());
	rayPipelineInfo.pGroups = m_rtShaderGroups.data();

	// The ray tracing process can shoot rays from the camera, and a shadow ray can be shot from the
	// hit points of the camera rays, hence a recursion level of 2. This number should be kept as low
	// as possible for performance reasons. Even recursive ray tracing should be flattened into a loop
	// in the ray generation to avoid deep recursion.
	rayPipelineInfo.maxPipelineRayRecursionDepth = std::min(25u, m_rtProperties.maxRayRecursionDepth);  // Ray depth
	std::cout << "max depth: " << rayPipelineInfo.maxPipelineRayRecursionDepth << std::endl;

	rayPipelineInfo.layout = m_rtPipelineLayout;

	vkCreateRayTracingPipelinesKHR(engine->_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_rtPipeline);

	// Clean up shader modules after pipeline creation
	vkDestroyShaderModule(engine->_device, main, nullptr);
	vkDestroyShaderModule(engine->_device, miss, nullptr);
	vkDestroyShaderModule(engine->_device, miss2, nullptr);
	vkDestroyShaderModule(engine->_device, closestHit, nullptr);
}



void RaytracingHandler::createRtShaderBindingTable(VulkanEngine* engine) {
	uint32_t missCount{ 2 };
	uint32_t hitCount{ 1 };
	auto     handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = m_rtProperties.shaderGroupHandleSize;

	// The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
	uint32_t handleSizeAligned = nvh::align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

	m_rgenRegion.stride = nvh::align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
	m_rgenRegion.size = m_rgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
	m_missRegion.stride = handleSizeAligned;
	m_missRegion.size = nvh::align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
	m_hitRegion.stride = handleSizeAligned;
	m_hitRegion.size = nvh::align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

	// Get the shader group handles
	uint32_t             dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	auto result = vkGetRayTracingShaderGroupHandlesKHR(engine->_device, m_rtPipeline, 0, handleCount, dataSize, handles.data());
	assert(result == VK_SUCCESS);

	// Allocate a buffer for storing the SBT.
	VkDeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;
	m_rtSBTBuffer = create_buffer(&engine->_device, &engine->_allocator,sbtSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);
	//m_debug.setObjectName(m_rtSBTBuffer.buffer, std::string("SBT"));  // Give it a debug name for NSight.

	// Find the SBT addresses of each group
	VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_rtSBTBuffer.buffer };
	VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(engine->_device, &info);
	m_rgenRegion.deviceAddress = sbtAddress;
	m_missRegion.deviceAddress = sbtAddress + m_rgenRegion.size;
	m_hitRegion.deviceAddress = sbtAddress + m_rgenRegion.size + m_missRegion.size;

	// Helper to retrieve the handle data
	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	// Step 1: Map the memory
	void* dataPtr = nullptr;
	VkResult mapResult = vmaMapMemory(engine->_allocator, m_rtSBTBuffer.allocation, &dataPtr);
	assert(mapResult == VK_SUCCESS && "Failed to map SBT buffer memory!");

	uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(dataPtr);

	// Step 2: Copy data


	uint32_t handleIdx = 0;
	// Raygen
	uint8_t* pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);

	// Miss
	pData = pSBTBuffer + m_rgenRegion.size;
	for (uint32_t c = 0; c < missCount; c++)
	{
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += m_missRegion.stride;
	}

	// Hit
	pData = pSBTBuffer + m_rgenRegion.size + m_missRegion.size;
	for (uint32_t c = 0; c < hitCount; c++)
	{
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += m_hitRegion.stride;
	}

	// Step 3: Unmap the memory
	vmaUnmapMemory(engine->_allocator, m_rtSBTBuffer.allocation);
}


void RaytracingHandler::raytrace(VkCommandBuffer cmd, VulkanEngine* engine) {
	

	VkDescriptorSet globalDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, engine->_gpuSceneDataDescriptorLayout);

	

	// Map memory using vmaMapMemory
	void* mappedData = nullptr;
	VkResult result = vmaMapMemory(engine->_allocator, m_globalsBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for global uniform buffer!");
	}


	if (lastState != engine->accumulate && engine->accumulate) {
		engine->cameraMoved = true;
	}
	lastState = engine->accumulate;

	bool clearScreen = false;
	if (engine->cameraMoved) {
		clearScreen = true;
		currentRayCount = 0;
	}


	m_uniformMappedPtr->raytracingSettings.offlineMode = offlineMode ? 1 : 0;
	m_uniformMappedPtr->raytracingSettings.rayBudget = rayBudget;
	m_uniformMappedPtr->raytracingSettings.seed = uint32_t(std::rand());
	m_uniformMappedPtr->raytracingSettings.lightCount = engine->lightSources.size();
	m_uniformMappedPtr->clearScreen = clearScreen ? 1 : 0;
	m_uniformMappedPtr->raytracingSettings.currentRayCount = currentRayCount;
	currentRayCount += rayBudget;
	// Copy uniform data into the mapped memory
	std::memcpy(mappedData, m_uniformMappedPtr, sizeof(GlobalUniforms));

	// Unmap the memory after copying the data
	vmaUnmapMemory(engine->_allocator, m_globalsBuffer.allocation);



	// Bind the ray tracing pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);

	// Allocate a descriptor set for the uniform buffer
	VkDescriptorSet uniformsDescriptor = engine->get_current_frame()._frameDescriptors.allocate(engine->_device, m_descSetLayout);

	// Bind the uniform buffer to the descriptor set using the DescriptorWriter
	DescriptorWriter writer;
	writer.write_buffer(0, m_globalsBuffer.buffer, sizeof(GlobalUniforms), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_buffer(
		1,
		m_bObjDesc.buffer,                      // the buffer containing the ObjDesc array
		sizeof(ObjDesc)*objDescs.size(),    // range
		0,                                       // offset
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
		objDescs.size()
	);
	writer.write_texture_array(2, engine->textureImages, engine->textureSamplers, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_buffer(
		3,
		m_bmatDesc.buffer,
		sizeof(MaterialConstants) * materialConstants.size(),
		0,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		materialConstants.size());
	writer.write_cube_map(
		4, 
		engine->_cubeMap, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	//writer.write_buffer(
	//	5,
	//	engine->_lightingDataBuffer.buffer,
	//	sizeof(ShaderLightSource) * engine->lightSources.size(),
	//	0,
	//	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	//	engine->lightSources.size());

	writer.update_set(engine->_device, uniformsDescriptor);

	DescriptorWriter writer2;
	writer2.write_image(0, engine->_gBuffer_albedo.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(1, engine->_gBuffer_normal.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(2, engine->_gBuffer_metallicRoughnes.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(3, engine->_gBuffer_Emissive.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(4, engine->_depthImage.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(5, engine->_colorHistory.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer2.write_image(6, engine->_depthHistory.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer2.update_set(engine->_device, engine->_gBufferDescriptors);
	// Bind the ray tracing descriptor sets
	std::vector<VkDescriptorSet> descSets{ m_rtDescSet, uniformsDescriptor, engine->_gBufferDescriptors, engine->_lightsourceDescriptorSet };
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_rtPipelineLayout,
		0,
		static_cast<uint32_t>(descSets.size()),
		descSets.data(),
		0,
		nullptr
	);


	engine->_raytracePushConstant.lightColor = engine->sceneData.sunlightColor;
	engine->_raytracePushConstant.lightIntensity = engine->_directionalLighting.intensity;
	engine->_raytracePushConstant.lightPosition = engine->sceneData.sunlightDirection;
	engine->_raytracePushConstant.ambientColor = engine->sceneData.ambientColor;
	engine->_raytracePushConstant.useAccumulation = engine->accumulate && !engine->useSVGF;
	// Push constants to the pipeline if necessary
	vkCmdPushConstants(
		cmd,
		m_rtPipelineLayout,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
		0,
		sizeof(PushConstantRay),
		&engine->_raytracePushConstant
	);

	// Issue the ray tracing command
	vkCmdTraceRaysKHR(
		cmd,
		&m_rgenRegion,
		&m_missRegion,
		&m_hitRegion,
		&m_callRegion,
		engine->_drawExtent.width,
		engine->_drawExtent.height,
		1
	);
	
}

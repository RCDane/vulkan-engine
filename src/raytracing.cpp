#include <raytracing.h>
#include <vk_engine.h>
#include <vulkan/vulkan_core.h>
#include <algorithm> // Add this include for std::max
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

	


	return true;
}



BlasInput RaytracingBuilder::objectToVkGeometry(VulkanEngine *engine,const RenderObject& object) {
	VkDeviceAddress vertexAddress = object.vertexBufferAddress;
	VkDeviceAddress indexAddress = getBufferDeviceAddress(engine->_device, object.indexBuffer);

	uint32_t maxPrimitiveCoun = object.indexCount / 3;
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertexAddress; 
	triangles.vertexStride = sizeof(Vertex);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.maxVertex = object.indexCount;

	VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeom.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = 0;
	offset.primitiveCount = maxPrimitiveCoun;
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
	// void addGeometry(const VkAccelerationStructureGeometryKHR& asGeom, const VkAccelerationStructureBuildRangeInfoKHR& offset);
	// void addGeometry(const AccelerationStructureGeometryInfo& asGeom);

	// AccelerationStructureGeometryInfo makeInstanceGeometry(size_t numInstances, VkDeviceAddress instanceBufferAddr);

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
		// assert(alignedSize == buildInfo.sizeInfo.buildScratchSize);  // Make sure it was already aligned
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

		std::string toString() const;
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
		data.buildInfo.scratchData.deviceAddress = scratchAddress[m_currentBlasIdx % scratchAddress.size()];
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
		numScratch = std::min(numScratch, buildData.size());
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
void BlasBuilder::getScratchAddresses(VkDeviceSize                                             hintMaxBudget,
	const std::vector<AccelerationStructureBuildData>& buildData,
	VkDeviceAddress               scratchBufferAddress,
	std::vector<VkDeviceAddress>& scratchAddresses,
	uint32_t                      minAlignment /*=128*/)
{
	ScratchSizeInfo sizeInfo = calculateScratchAlignedSizes(buildData, minAlignment);
	VkDeviceSize    maxScratch = sizeInfo.maxScratch;
	VkDeviceSize    totalScratch = sizeInfo.totalScratch;

	// Strategy 1: scratch was large enough for all BLAS, return the addresses in order
	if (totalScratch < hintMaxBudget)
	{
		VkDeviceAddress address = {};
		for (auto& buildInfo : buildData)
		{
			scratchAddresses.push_back(scratchBufferAddress + address);
			VkDeviceSize alignedSize = nvh::align_up(buildInfo.sizeInfo.buildScratchSize, minAlignment);
			address += alignedSize;
		}
	}
	// Strategy 2: there are n-times the max scratch fitting in the budget
	else
	{
		// Make sure there is at least one scratch buffer, and not more than the number of BLAS
		uint64_t numScratch = std::max(uint64_t(1), hintMaxBudget / maxScratch);
		numScratch = std::min(numScratch, buildData.size());

		VkDeviceAddress address = {};
		for (int i = 0; i < numScratch; i++)
		{
			scratchAddresses.push_back(scratchBufferAddress + address);
			address += maxScratch;
		}
	}
}


void RaytracingBuilder::buildBlas(VulkanEngine* engine, const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags){
	VkCommandBuffer commandBuffer = engine->_immCommandBuffer;
	auto numBlas = static_cast<uint32_t>(input.size());
	VkDeviceSize asTotalSize{ 0 };
	VkDeviceSize maxScratchSize{ 0 };

	std::vector<AccelerationStructureBuildData> blasBuildData(numBlas);
	m_blas.resize(numBlas);

	for (uint32_t idx = 0; idx < numBlas; idx++) {
		blasBuildData[idx].asType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		blasBuildData[idx].asGeometry = input[idx].asGeometry;
		blasBuildData[idx].asBuildRangeInfo = input[idx].asBuildOffsetInfo;

		auto sizeInfo = blasBuildData[idx].finalizeGeometry(engine->_device, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		maxScratchSize = std::max(maxScratchSize, sizeInfo.buildScratchSize);
	}

	VkDeviceSize hintMaxBudget{ 256'000'000 };  // 256 MB

	AllocatedBuffer blasScratchBuffer;
	bool hasCompaction = hasFlag(flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

	BlasBuilder blasBuilder(&engine->globalDescriptorAllocator, engine->_device);

	uint32_t minAlignment = 128; /*m_rtASProperties.minAccelerationStructureScratchOffsetAlignment*/
	// 1) finding the largest scratch size
	VkDeviceSize scratchSize = blasBuilder.getScratchSize(hintMaxBudget, blasBuildData, minAlignment);
	// 2) allocating the scratch buffer
	blasScratchBuffer =
		create_buffer(&engine->_device, &engine->_allocator,scratchSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	// 3) getting the device address for the scratch buffer
	std::vector<VkDeviceAddress> scratchAddresses;
	VkDeviceAddress address = getBufferDeviceAddress(engine->_device, blasScratchBuffer.buffer);
	blasBuilder.getScratchAddresses(hintMaxBudget, blasBuildData, address, scratchAddresses, minAlignment);

	
		
	engine->immediate_submit([&](VkCommandBuffer cmd) {
		blasBuilder.cmdCreateParallelBlas(cmd, blasBuildData, m_blas, scratchAddresses, hintMaxBudget);
		});
		
	if (hasCompaction)
	{
		engine->immediate_submit([&](VkCommandBuffer cmd) {
			blasBuilder.cmdCreateParallelBlas(cmd, blasBuildData, m_blas, scratchAddresses, hintMaxBudget);
			blasBuilder.cmdCompactBlas(cmd, blasBuildData, m_blas);
			});
			
		blasBuilder.destroyNonCompactedBlas();
	}

	destroy_buffer(blasScratchBuffer);
}

bool  RaytracingHandler::setup(DrawContext drawContext) {

	m_models = &drawContext;
	return true;
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
	m_rtBuilder.buildBlas(engine,allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}
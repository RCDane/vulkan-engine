#include <vk_buffers.h>
#include <vk_types.h>
#include <vulkan/vulkan.h>


AllocatedBuffer create_buffer(VkDevice *device, VmaAllocator *allocator,size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBufferCreateFlagBits flags)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.flags = flags;
	bufferInfo.usage = usage; 

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	
	
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(*allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));
	newBuffer.allocator = allocator;

	return newBuffer;
}

void destroy_buffer(AllocatedBuffer buffer)
{
	vmaDestroyBuffer(*buffer.allocator, buffer.buffer, buffer.allocation);
}
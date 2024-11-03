#pragma once
#ifndef VK_BUFFER_H
#define VK_BUFFER_H


#include <vulkan/vulkan.h>
#include <vk_types.h>
inline VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer)
{
	if (buffer == VK_NULL_HANDLE)
		return 0ULL;

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = buffer;
	return vkGetBufferDeviceAddress(device, &info);
}

AllocatedBuffer create_buffer(VkDevice* device, VmaAllocator* allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
void destroy_buffer(AllocatedBuffer buffer);





#endif // VK_BUFFER_H
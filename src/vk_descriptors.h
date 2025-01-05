#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> _bindingFlags;

    void add_binding(uint32_t binding, VkDescriptorType type, VkDescriptorBindingFlags bindingFlags = NULL, int count=1);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct BindlessDescriptor {
	std::vector<VkDescriptorSet> sets;
};



struct DescriptorAllocator {

    struct PoolSizeRatio{
		VkDescriptorType type;
		float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, VmaAllocator *allocator, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios, bool updating = false);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

    AccelKHR createAcceleration(VkDevice device, const VkAccelerationStructureCreateInfoKHR& accel_);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    bool _updating;

	VmaAllocator *_allocator;
	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};



struct DescriptorWriter {
    std::vector<VkDescriptorImageInfo> textureInfos;
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
    void write_buffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 
	void write_acceleration_structure(int binding, VkWriteDescriptorSetAccelerationStructureKHR as);
    void write_texture_array(int binding, const std::vector<VkImageView>& images, const std::vector<VkSampler> samplers, VkDescriptorType type);
    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};
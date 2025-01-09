#include <vk_descriptors.h>
#include <vk_buffers.h>
void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, VkDescriptorBindingFlags bindingFlags, int count)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = count;
    newbind.descriptorType = type;
    

    if (bindingFlags != 0) {
        _bindingFlags.push_back(bindingFlags);
    }
    else {
        // even if bindingFlags == 0, push it to keep indexing consistent
        _bindingFlags.push_back(0);
    }

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
    _bindingFlags.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(
    VkDevice device,
    VkShaderStageFlags shaderStages,
    void* pNext,
    VkDescriptorSetLayoutCreateFlags flags)
{
    // Fill in the stage flags if needed
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    // 1) Create an extended structure that holds the binding flags
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT CreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .pNext = nullptr,
        .bindingCount = static_cast<uint32_t>(_bindingFlags.size()),
		.pBindingFlags = _bindingFlags.size() > 0 ? _bindingFlags.data() : nullptr
    };

    


    // 2) Create the normal layout info
    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &CreateInfo,  // attach the extended binding flags
        .flags = flags,
        .bindingCount = (uint32_t)bindings.size(),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout setLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));

    return setLayout;
}


void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios){
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize {
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.flags = 0;
	pool_info.maxSets = maxSets;
	pool_info.poolSizeCount = (uint32_t)poolSizes.size();
	pool_info.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device,pool,nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device){
    VkDescriptorPool newPool;
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    }
    else {
        newPool = create_pool(device, setsPerPool, ratios);

        setsPerPool = setsPerPool * 1.5;
        if (setsPerPool > 4092){
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setCount)
		});
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = setCount;
	pool_info.poolSizeCount = (uint32_t)poolSizes.size();
	pool_info.pPoolSizes = poolSizes.data();

	if (_updating) {
		pool_info.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
	}

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
    return newPool;
}

void DescriptorAllocatorGrowable::init(VkDevice device, VmaAllocator *allocator, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios, bool updating){
    ratios.clear();
	_allocator = allocator;
    for (auto r : poolRatios){
        ratios.push_back(r);
    }

	_updating = updating;
    VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

    setsPerPool = maxSets * 1.5;

    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{ 
    for (auto p : readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
	for (auto p : readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
    readyPools.clear();
	for (auto p : fullPools) {
		vkDestroyDescriptorPool(device,p,nullptr);
    }
    fullPools.clear();
}


VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext ){
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};

    allocInfo.pNext = pNext;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL){
        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type, int count)
{
    // increase size such that it is divisible by 16

    int minAlignment = 16;
    VkDeviceSize ofset = (offset / minAlignment) * minAlignment;
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
		});

	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_texture_array(int binding, const std::vector<VkImageView>& images, const std::vector<VkSampler> samplers, VkDescriptorType type) {

    if (images.size() != samplers.size()) {
        throw std::runtime_error("Mismatch between number of images and samplers.");
    }
    // Create the array of descriptor image infos
	textureInfos.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        textureInfos[i] = {
            .sampler = samplers[i],
            .imageView = images[i],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    // Configure the descriptor write
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = VK_NULL_HANDLE, // Assigned later during update_set
        .dstBinding = (uint32_t)binding,
        .descriptorCount = (uint32_t)images.size(),
        .descriptorType = type,
        .pImageInfo = textureInfos.data(),
    };

    // Append to the writes list
    writes.push_back(write);
}

void DescriptorWriter::write_image(int binding,VkImageView image, VkSampler sampler,  VkImageLayout layout, VkDescriptorType type)
{
    VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_acceleration_structure(int binding, VkWriteDescriptorSetAccelerationStructureKHR as)
{
	
	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	write.pNext = &as;
	writes.push_back(write);
}


void DescriptorWriter::clear()
{
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

AccelKHR DescriptorAllocatorGrowable::createAcceleration(VkDevice device,const VkAccelerationStructureCreateInfoKHR& accel_)
{
    AccelKHR resultAccel;

    assert(accel_.size > 0 && "Acceleration structure size must be greater than zero.");

    // Allocating the buffer to hold the acceleration structure
    resultAccel.buffer.buffer = create_buffer(&device, _allocator, accel_.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);


    assert(resultAccel.buffer.buffer.buffer != VK_NULL_HANDLE && "Failed to create acceleration structure buffer.");

    // Setting the buffer
    VkAccelerationStructureCreateInfoKHR accel = accel_;
    accel.buffer = resultAccel.buffer.buffer.buffer;
	resultAccel.buffer.deviceAddress = getBufferDeviceAddress(device, resultAccel.buffer.buffer.buffer);

    assert(resultAccel.buffer.deviceAddress != 0 && "Invalid device address for acceleration structure buffer.");

    // Create the acceleration structure
    VkResult result = vkCreateAccelerationStructureKHR(device, &accel, nullptr, &resultAccel.accel);

    assert(result == VK_SUCCESS && "Failed to create acceleration structure.");
    assert(resultAccel.accel != VK_NULL_HANDLE && "Acceleration structure handle is null after creation.");


    if (vkGetAccelerationStructureDeviceAddressKHR != nullptr)
    {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            nullptr,
            resultAccel.accel
        };
        resultAccel.buffer.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &info);

        assert(resultAccel.buffer.deviceAddress != 0 && "Failed to retrieve device address for acceleration structure.");
    }
    else
    {
        assert(false && "vkGetAccelerationStructureDeviceAddressKHR is not available.");
    }

    return resultAccel;
}
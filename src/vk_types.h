// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <stdexcept>
#include <format>
#include <iostream>
#include <stdlib.h>
#include <string>
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err<0) {                                                   \
            std::cout << std::format("Detected Vulkan error: {}", string_VkResult(err)) << ""; \
            abort();                                                    \
        }                                                               \
    } while (0)


struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    std::string name;
};
struct CubeMap {
    AllocatedImage image;
	VkSampler sampler;
    std::string name;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VmaAllocator *allocator;
};
struct AccelBuffer {
    AllocatedBuffer buffer;
    VkDeviceAddress deviceAddress;
};

struct AccelKHR
{
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    AccelBuffer               buffer;

	void destroy(VkDevice device)
	{
		vkDestroyAccelerationStructureKHR(device, accel, nullptr);
		vmaDestroyBuffer(*buffer.buffer.allocator, buffer.buffer.buffer, buffer.buffer.allocation);
	}
};
struct PushConstantReproj
{
    glm::mat4 projInverse;
    glm::mat4 viewInverse;
    glm::mat4 oldViewProj;
    glm::ivec2 imageSize;
};
struct PushConstantAtrous {
    glm::ivec2 imageSize;
    int stepSize;
    float phiColor;
    float phiNormal;
};

struct PushConstantImageSize
{
    glm::ivec2 imageSize;
};
enum LightType {
    Point = 0,
    Directional = 1
};

struct LightSource {
    LightType type;
    float intensity;
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float radius;
    float sunAngle;
};




struct alignas(16) ShaderLightSource {
    glm::vec3 position;
    float sunAngle;
    glm::vec3 color;
    float radius;
    glm::vec3 direction;
    float intensity;
    glm::vec2 padding;
    int type;
    float pdf;

	 // padding to align to 16 bytes

};
static_assert(sizeof(ShaderLightSource) == 64, "ShaderLightSource struct size should be 64 bytes.");

struct alignas(16) Vertex {
    glm::vec3 position;    // 12 bytes
    float padding1;        // 4 bytes to align to 16 bytes
    glm::vec3 normal;      // 12 bytes
    float padding2;        // 4 bytes to align to 16 bytes
    glm::vec2 uv;          // 8 bytes
    float padding3[2];     // 8 bytes to align to 16 bytes
    glm::vec4 color;       // 16 bytes
    glm::vec4 tangent;     // 16 bytes
};
static_assert(sizeof(Vertex) == 80, "Vertex struct size should be 96 bytes.");

// holds the resources needed for a mesh
struct GPUMeshBuffers {

    AllocatedBuffer indexBuffer;
    AllocatedBuffer indexBufferRaytracing;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
    VkDeviceAddress IndexBufferAddress;
    VkDeviceAddress IndexBufferAddressRaytracing;


    uint32_t vertexCount;
};
struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    int colorIdx;
    int normalIdx;
    int metalIdx;
    int emissiveIdx;
    glm::vec4 padding2;
};
static_assert(sizeof(MaterialConstants) == 64, "MaterialConstants struct size should be 48 bytes.");

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
    int hasTangents;
    int usePCF;
};

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};
struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialConstants* data;
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct ToneMappingSettings {
    float gamma;
};

struct DrawContext;
// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
};

struct MeshAsset;
struct DrawContext;
struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};


#pragma once


#include <vk_types.h>

#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>
#include "iridescence.h"
struct GLTFMaterial {
	MaterialInstance data;
};
struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};
struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    uint32_t startVertex;
    uint32_t vertexCount;
    uint32_t maxVertex;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
    bool hasTangents;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

struct LoadedGLTF : public IRenderable {

    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    VulkanEngine* creator;

	glm::mat4 rootTransform = glm::mat4(1.0f);

    ~LoadedGLTF() { clearAll(); };

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);

    // For gamejam only

private:

    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath);


CubeMap load_cube_map(VulkanEngine* engine, std::string_view filePath);
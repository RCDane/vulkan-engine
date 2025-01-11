﻿#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vk_loader.h>
#include <stdlib.h>
#include <functional> 
#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include <iostream>

#include "vk_engine.h"
#include "vk_types.h"
#include "vk_buffers.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <vulkan/vulkan_core.h>
#include <stdexcept>


std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                const std::string path(filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                               // specify LoadExternalBuffers, meaning all buffers
                                               // are already loaded into a vector.
                               [](auto& arg) {},
                               [&](fastgltf::sources::Vector& vector) {
                                   unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,true);

                                       stbi_image_free(data);
                                   }
                               } },
                    buffer.data);
            },
        },
        image.data);

    if (newImage.image == VK_NULL_HANDLE){
        return {};
    }
    else {
		newImage.name = image.name;
        return newImage;
    }
}



VkFilter extract_filter(fastgltf::Filter filter)
{
    switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}





std::unordered_map<size_t, int> textureMap;
bool textureMapInitialized = false;


uint32_t findMaxVertexIdx(std::span<uint32_t> indices) {
    return *std::max_element(indices.begin(), indices.end());
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath){
    fmt::print("Loading GLTF: {}\n", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser {};

    constexpr auto gltfOptions = 
            fastgltf::Options::DontRequireValidAssetMember | 
            fastgltf::Options::AllowDouble | 
            fastgltf::Options::LoadGLBBuffers | 
            fastgltf::Options::LoadExternalBuffers;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);
    
    fastgltf::Asset gltf;
    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF){
        auto load = parser.loadGLTF(&data, path.parent_path(),gltfOptions);
        if (load){
            gltf = std::move(load.get());
        }
        else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }            
    } else if (type == fastgltf::GltfType::GLB){
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    // we can stimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

    int materisals_size = std::max((int)gltf.materials.size(), 1);

    file.descriptorPool.init(engine->_device,&engine->_allocator, materisals_size, sizes,true);

    for (fastgltf::Sampler& sampler : gltf.samplers){
        VkSamplerCreateInfo sampl = 
            {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,};

        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode= extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // load all textures
    for (fastgltf::Image& image : gltf.images) {
    
        std::optional<AllocatedImage> img = load_image(engine, gltf, image);

		if (img.has_value()) {
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->_errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
    }

     // create buffer to hold the material data
    file.materialDataBuffer = create_buffer(&engine->_device, &engine->_allocator,sizeof(MaterialConstants) * materisals_size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;

    

    if (!textureMapInitialized) {
        engine->textureImages.emplace_back(engine->_whiteImage.imageView);
        engine->textureSamplers.emplace_back(engine->_defaultSamplerLinear);
        engine->textureImages.emplace_back(engine->_whiteImage.imageView);
        engine->textureSamplers.emplace_back(engine->_defaultSamplerLinear);
        engine->textureImages.emplace_back(engine->_defaultNormalMap.imageView);
        engine->textureSamplers.emplace_back(engine->_defaultSamplerLinear);
		textureMapInitialized = true;

    }


	int materialStart = engine->textureImages.size();
    MaterialConstants* sceneMaterialConstants = (MaterialConstants*)file.materialDataBuffer.info.pMappedData;

    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;


        MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

        constants.colorIdx = 0;
        constants.metalIdx = 1;
        constants.normalIdx = 2;
        
        // write material parameters to buffer

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;
        materialResources.normalImage = engine->_defaultNormalMap;
        materialResources.normalSampler = engine->_defaultSamplerLinear;
        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(MaterialConstants);
        materialResources.constants = constants;
        
        
        std::hash<std::string> hash_fn;
		std::string name = path.string();
        

        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
            materialResources.colorImage = images[img];
            materialResources.colorSampler = file.samplers[sampler];
            uint32_t idx = 0;

            size_t colorTextureIndex = mat.pbrData.baseColorTexture.value().textureIndex;

            size_t hash = hash_fn(name + std::to_string(colorTextureIndex));



            if (!textureMap.contains(hash)) {
				idx = engine->textureImages.size();
                engine->textureImages.emplace_back(materialResources.colorImage.imageView);
                engine->textureSamplers.emplace_back(materialResources.colorSampler);
				textureMap.emplace(hash, idx);

            }
            else {
                idx = textureMap[hash];
                materialResources.colorImage.imageView = engine->textureImages[idx];
                materialResources.colorSampler = engine->textureSamplers[idx];
            }
            constants.colorIdx = idx;
        }
        
        // Ensure that metal/roughness textures are not loaded multiple times
        if (mat.pbrData.metallicRoughnessTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();
            materialResources.metalRoughImage = images[img];
            materialResources.metalRoughSampler = file.samplers[sampler];
			uint32_t idx = 0;


            size_t metalTextureIndex = mat.pbrData.metallicRoughnessTexture.value().textureIndex;

            size_t hash = hash_fn(name + std::to_string(metalTextureIndex));

            if (!textureMap.contains(hash)) {
                idx = engine->textureImages.size();
                engine->textureImages.emplace_back(materialResources.metalRoughImage.imageView);
                engine->textureSamplers.emplace_back(materialResources.metalRoughSampler);
                textureMap.emplace(hash, idx);
            }
            else {
                idx = textureMap[hash];
                materialResources.metalRoughImage.imageView = engine->textureImages[idx];
                materialResources.metalRoughSampler = engine->textureSamplers[idx];
            }
            constants.metalIdx = idx;

        }

        // Ensure that normal maps are not loaded multiple times
        if (mat.normalTexture.has_value()) {
            size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();
            materialResources.normalImage = images[img];
            materialResources.normalSampler = file.samplers[sampler];
            uint32_t idx = 0;

            size_t normalTextureIndex = mat.normalTexture.value().textureIndex;

            size_t hash = hash_fn(name + std::to_string(normalTextureIndex));

            if (!textureMap.contains(hash)) {
                idx = engine->textureImages.size();
                engine->textureImages.emplace_back(materialResources.normalImage.imageView);
                engine->textureSamplers.emplace_back(materialResources.normalSampler);
                textureMap.emplace(hash, idx);
            }
            else {
                idx = textureMap[hash];
                materialResources.normalImage.imageView = engine->textureImages[idx];
                materialResources.normalSampler = engine->textureSamplers[idx];
            }
            constants.normalIdx = idx;
        }

        sceneMaterialConstants[data_index] = constants;
        
        // build material
        newMat->data = engine->metalRoughMaterial.write_material(engine, passType, materialResources, file.descriptorPool);
		newMat->data.data = &sceneMaterialConstants[data_index];
        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<uint32_t> raytracingIndices;
    std::vector<Vertex> vertices;

    bool hasTangents = false;
    try {

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;
        
        // Clear the mesh arrays for the new mesh
        indices.clear();
        vertices.clear();
        int lastMeshesIndex = 0;

        // Clear surfaces for the new mesh
        newmesh->surfaces.clear();

        for (auto&& p : mesh.primitives) {
            
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);
            newSurface.hasTangents = false;
            size_t initial_vtx = vertices.size();
            newSurface.startVertex = static_cast<uint32_t>(initial_vtx);

            // Load indices
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);
				raytracingIndices.reserve(raytracingIndices.size() + indexaccessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx); // Offset by initial_vtx
                        raytracingIndices.push_back(idx); 

                    });
            }

            int primMaterial = 0;
            // Assign material
            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
				primMaterial = p.materialIndex.value();
            }
            else {
                newSurface.material = materials[0];
            }


            // Load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv = glm::vec2(0.0f);
						newvtx.materialIndex = primMaterial;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // Load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // Load vertex tangents
            auto tangents = p.findAttribute("TANGENT");
            if (tangents != p.attributes.end()) {
                newSurface.hasTangents = true;
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*tangents).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].tangent = v;
                    });
            }

            // Load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv = v;
                    });
            }

            // Load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            

            // Compute min/max bounds
            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (size_t i = initial_vtx; i < vertices.size(); i++) {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }
            // Calculate origin and extents from the min/max, use extent length for radius
            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

            newSurface.maxVertex = *std::max_element(indices.begin(), indices.end());
            fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
            newSurface.vertexCount = static_cast<uint32_t>(posAccessor.count);



            newmesh->surfaces.push_back(newSurface);
        }

        if (!hasTangents) {
            // We have to calculate tangents ourselves
        }

        newmesh->meshBuffers = engine->uploadMesh(indices, vertices, raytracingIndices);
    }
    }
    catch (const std::exception& e){
        std::cout << "Exception " << e.what() << std::endl;
    }
        // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor { 
                        [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                        [&](fastgltf::Node::TRS transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4 { 1.f });
        }
    }
    return scene;
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx){
    for (auto& n : topNodes){
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->_device;

    descriptorPool.destroy_pools(dv);
    destroy_buffer(materialDataBuffer);

    for (auto& [k, v] : meshes) {

		destroy_buffer(v->meshBuffers.indexBuffer);
		destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) {
        
        if (v.image == creator->_errorCheckerboardImage.image) {
            //dont destroy the default images
            continue;
        }
        creator->destroy_image(v);
    }

	for (auto& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
    }
}


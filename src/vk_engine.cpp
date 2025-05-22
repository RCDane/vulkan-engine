//> includes
#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <string>
#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>

#include <vk_extensions.h>
#include <glm/gtx/transform.hpp>


#include "glm/ext/vector_float3.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "shadows.h"
#include <raytracing.h>
#include <vk_buffers.h>
#include "vk_descriptors.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <vk_loader.h>
#include "glm/gtc/random.hpp"
//bootstrap library
#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <thread>
#include <vulkan/vulkan_core.h>
#include <chrono>

#include <filesystem>
#include <iostream>

#include <svgf.h>

//#include <windows.h>
//#include <tchar.h>

constexpr bool bUseValidationLayers = true;


VulkanEngine* loadedEngine = nullptr;




VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

	std::srand(std::time({}));


    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        _windowExtent.width,
        _windowExtent.height,
        window_flags);
	//SDL_Renderer* renderer = SDL_CreateRenderer(_window, "vulkan");
	
	
	//SDL_SetWindowFullscreen(_window, 0);
	//SDL_SetRenderVSync(renderer, 0);
    init_vulkan();

    init_swapchain();
    
    init_commands();

    init_sync_structures();

	// Need to find correct placement for this
	immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image_relaxed(cmd, _gBuffer_albedo.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkutil::transition_image_relaxed(cmd, _gBuffer_normal.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkutil::transition_image_relaxed(cmd, _gBuffer_metallicRoughnes.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkutil::transition_image_relaxed(cmd, _gBuffer_Emissive.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkutil::transition_image_relaxed(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		vkutil::transition_image_relaxed(cmd, _depthHistory.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		vkutil::transition_image_relaxed(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		});


	init_descriptors();	
	//init_background_pipelines();

	init_pipelines();

	init_timestamp_queries();

	init_imgui();

	init_default_data();

	//DWORD dwError, dwPriClass;
	//dwPriClass = GetPriorityClass(GetCurrentProcess());

	//_tprintf(TEXT("Current priority class is 0x%x\n"), dwPriClass);



	_cubeMap = load_cube_map(this, "../assets/black_skybox");




	//std::string spherePath = { "../assets/simple_sphere.glb"};
	//auto sphereFile = loadGltf(this, spherePath);

	//assert(sphereFile.has_value());
	//

	//loadedScenes["sphere"] = *sphereFile;
	//loadedScenes["sphere"]->rootTransform = glm::scale(glm::vec3(10.0f)) * loadedScenes["sphere"]->rootTransform;
	//loadedScenes["sphere"]->rootTransform = glm::translate(glm::vec3(0.0f, 20.0f, 0.0f)) * loadedScenes["sphere"]->rootTransform;


	




	//std::string helmetPath = { "../assets/SciFiHelmet/SciFiHelmet.glb"};
	//std::optional<std::shared_ptr<LoadedGLTF>> helmetFile = loadGltf(this, helmetPath);

	//assert(helmetFile.has_value());
	//

	//loadedScenes["helmet"] = *helmetFile;
	//loadedScenes["helmet"]->rootTransform = glm::scale(glm::vec3(10.0f)) * loadedScenes["helmet"]->rootTransform;
	//loadedScenes["helmet"]->rootTransform = glm::translate(glm::vec3(0.0f, 20.0f, 0.0f)) * loadedScenes["helmet"]->rootTransform;

	std::string sponzaPath = { "../assets/sponza.glb" };
	auto sponzaFile = loadGltf(this, sponzaPath);
	assert(sponzaFile.has_value());
	loadedScenes["sponza"] = *sponzaFile;

	//std::string sphereTestPath = { "../assets/sphere test.glb" };
	//auto sphereTestFile = loadGltf(this, sphereTestPath);
	//assert(sphereTestFile.has_value());
	//loadedScenes["sphereTest"] = *sphereTestFile;


	for (auto const& [key, scene] : loadedScenes) {
		if (scene->camera) {
			mainCamera = scene->camera;
			mainCamera->zNear = 0.1f;
			mainCamera->zFar = 300.0;
		}
		if (scene->lightSources.size() > 0) {
			lightSources.insert(lightSources.end(),
				scene->lightSources.begin(),
				scene->lightSources.end());
		}
	}

	if (mainCamera == NULL) {
		mainCamera = std::make_shared<Camera>();
		mainCamera->velocity = glm::vec3(0.f);
		mainCamera->position = glm::vec3(0.f, 0.f, 0.f);
		mainCamera->pitch = 0;
		mainCamera->yaw = 0;
		mainCamera->aspectRatio = 1;
		mainCamera->fov = 70;
		mainCamera->zNear = 0.001f;
		mainCamera->zFar = 100.0f;
		mainCamera->isPerspective = true;
	}




	// Multiple dragons
	//std::string plane_path = { "../assets/unit_plane.glb" };
	//auto planeFile = loadGltf(this, plane_path);
	//assert(planeFile.has_value());
	//loadedScenes["unit_plane"] = *planeFile;
	//loadedScenes["unit_plane"]->rootTransform = glm::scale(glm::vec3(45)) * loadedScenes["unit_plane"]->rootTransform;
	//loadedScenes["unit_plane"]->rootTransform = glm::translate(glm::vec3(20.f, 0.0f, 0.0f)) * loadedScenes["unit_plane"]->rootTransform;



	//std::string dragonPath = { "../assets/dragon.glb" };
	//auto dragonFile = loadGltf(this, dragonPath);
	//assert(dragonFile.has_value());
	//loadedScenes["dragon"] = *dragonFile;
	//loadedScenes["dragon"]->rootTransform = glm::scale(glm::vec3(30.0f)) * loadedScenes["dragon"]->rootTransform;
	//loadedScenes["dragon"]->rootTransform = glm::translate(glm::vec3(0, 0, 0)) * loadedScenes["dragon"]->rootTransform;


		//some default lighting parameters
	glm::vec4 sunDirection = glm::vec4(0.f, -1.0f, 0.0f, 1.f);

	sceneData.ambientColor = glm::vec4(0);
	sceneData.sunlightColor = glm::vec4(lightSources[0]->color, 0);
	sceneData.sunlightDirection = glm::vec4(lightSources[0]->direction, 0);

	pointLight.color = glm::vec4(1.f);
	pointLight.position = glm::vec3(0.f, 20.f, 0.f);
	pointLight.intensity = 2.f;

	_directionalLighting.color = glm::vec4(lightSources[0]->color, 0);;
	_directionalLighting.direction = lightSources[0]->direction;
	_directionalLighting.intensity = 1.f;

	_raytracePushConstant.clearColor = glm::vec4(0.1, 0.2, 0.4, 0.97);
	_raytracePushConstant.lightColor = glm::vec4(1, 1, 1, 1);
	_raytracePushConstant.lightPosition = sunDirection;
	_raytracePushConstant.lightType = 1;
	_raytracePushConstant.lightIntensity = 2.f;


	DescriptorWriter writer;

	writer.write_texture_array(0, textureImages, textureSamplers, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_cube_map(1, _cubeMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_device, _textureArrayDescriptor);
	writer.clear();
	_raytracingHandler.init_raytracing(this);



	writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_cube_map(1, _cubeMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.update_set(_device, _drawImageDescriptors);

	writer.clear();

	writer.write_image(0, _colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, _drawImage.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(_device, _tonemappingImageDescriptors);

	update_scene();

	_raytracingHandler.setup(this);
	_raytracingHandler.createDescriptorSetLayout(this);
	_raytracingHandler.createBottomLevelAS(this);
	_raytracingHandler.createTopLevelAS(this);
	_raytracingHandler.prepareModelData(this);

	_raytracingHandler.createRtDescriptorSet(this);
	_raytracingHandler.createRtPipeline(this);

	_raytracingHandler.createRtShaderBindingTable(this);


	_svgfHandler.init(this, _windowExtent);

	// everything went fine

	prepare_lighting_data();

    _isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
		// wait for device to idle, ensures that no commands are being run when cleaning up.
        vkDeviceWaitIdle(_device);
 		loadedScenes.clear();

        for (int i = 0; i < FRAME_OVERLAP; i++){
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
        
            //destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device ,_frames[i]._swapchainSemaphore, nullptr);

			_frames[i]._deletionQueue.flush();
        }

		for (auto& mesh : testMeshes) {
			destroy_buffer(mesh->meshBuffers.indexBuffer);
			destroy_buffer(mesh->meshBuffers.vertexBuffer);
		}

		_raytracingHandler.cleanup(_device);


		_mainDeletionQueue.flush();
		vkDestroyQueryPool(_device, _timestampQueryPool, nullptr);
        destroy_swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void extractFrustumPlanes(const glm::mat4& vpMatrix, glm::vec4 planes[6]) {
	// Left Plane
	planes[0] = glm::vec4(
		vpMatrix[0][3] + vpMatrix[0][0],
		vpMatrix[1][3] + vpMatrix[1][0],
		vpMatrix[2][3] + vpMatrix[2][0],
		vpMatrix[3][3] + vpMatrix[3][0]);
	// Right Plane
	planes[1] = glm::vec4(
		vpMatrix[0][3] - vpMatrix[0][0],
		vpMatrix[1][3] - vpMatrix[1][0],
		vpMatrix[2][3] - vpMatrix[2][0],
		vpMatrix[3][3] - vpMatrix[3][0]);
	// Bottom Plane
	planes[2] = glm::vec4(
		vpMatrix[0][3] + vpMatrix[0][1],
		vpMatrix[1][3] + vpMatrix[1][1],
		vpMatrix[2][3] + vpMatrix[2][1],
		vpMatrix[3][3] + vpMatrix[3][1]);
	// Top Plane
	planes[3] = glm::vec4(
		vpMatrix[0][3] - vpMatrix[0][1],
		vpMatrix[1][3] - vpMatrix[1][1],
		vpMatrix[2][3] - vpMatrix[2][1],
		vpMatrix[3][3] - vpMatrix[3][1]);
	// Near Plane
	planes[4] = glm::vec4(
		vpMatrix[0][3] + vpMatrix[0][2],
		vpMatrix[1][3] + vpMatrix[1][2],
		vpMatrix[2][3] + vpMatrix[2][2],
		vpMatrix[3][3] + vpMatrix[3][2]);
	// Far Plane
	planes[5] = glm::vec4(
		vpMatrix[0][3] - vpMatrix[0][2],
		vpMatrix[1][3] - vpMatrix[1][2],
		vpMatrix[2][3] - vpMatrix[2][2],
		vpMatrix[3][3] - vpMatrix[3][2]);

	// Normalize the planes
	for (int i = 0; i < 6; i++) {
		float length = glm::length(glm::vec3(planes[i]));
		planes[i] /= length;
	}
}
bool is_visible(const RenderObject& obj, const glm::mat4& viewProjMatrix) {
	// Extract frustum planes
	glm::vec4 planes[6];
	extractFrustumPlanes(viewProjMatrix, planes);

	// Get the object's model matrix
	glm::mat4 modelMatrix = obj.transform;

	// Compute the object's bounding sphere center in world space
	glm::vec3 centerWorld = glm::vec3(modelMatrix * glm::vec4(obj.bounds.origin, 1.0f));

	// Extract scaling factors from the model matrix
	glm::vec3 scale;
	scale.x = glm::length(glm::vec3(modelMatrix[0]));
	scale.y = glm::length(glm::vec3(modelMatrix[1]));
	scale.z = glm::length(glm::vec3(modelMatrix[2]));

	// Determine the maximum scale to uniformly scale the radius
	float maxScale = std::max({ scale.x, scale.y,scale.z });
	float scaledRadius = obj.bounds.sphereRadius * maxScale;



	// Check against each frustum plane
	for (int i = 0; i < 6; i++) {
		float distance = glm::dot(glm::vec3(planes[i]), centerWorld) + planes[i].w;
		if (distance < -scaledRadius) {
			// Completely outside the frustum
			return false;
		}
	}
	// Inside or intersects the frustum
	return true;
}


void AddCmdMarker(VkCommandBuffer cmd, std::string name) {
	VkDebugMarkerMarkerInfoEXT markerInfo;
	markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
	markerInfo.pMarkerName = name.c_str();
	vkCmdDebugMarkerBeginEXT(cmd, &markerInfo);
}

void RemoveMarker(VkCommandBuffer cmd) {
	vkCmdDebugMarkerEndEXT(cmd);
}
void NameImage(VkDevice device, VkImage image, std::string name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo;
	nameInfo.sType = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
	//nameInfo.objectHandle = image;
	nameInfo.pObjectName = name.c_str();
	//vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}

void NameImageView(VkDevice device, VkImageView image, std::string name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo;
	nameInfo.sType = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
	//nameInfo.objectHandle = &image;
	nameInfo.pObjectName = name.c_str();
	//vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}


void WaitAll(VkCommandBuffer cmd) {
	//VkMemoryBarrier2 fullBarrier2{
	//.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
	//.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
	//.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
	//.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
	//.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT
	//};

	//VkDependencyInfo depInfo{
	//	.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	//	.memoryBarrierCount = 1,
	//	.pMemoryBarriers = &fullBarrier2
	//};

	//vkCmdPipelineBarrier2(cmd, &depInfo);
}

#include <stb_image_write.h>

void write_png_image(
	const float* src,          // ← now float*
	VkExtent2D      size,
	int             channelCount, // e.g. 4 for RGBA
	const std::string& name
) {
	int width = size.width;
	int height = size.height;
	int numFloats = width * height * channelCount;

	// Prepare a byte buffer the same total size:
	std::vector<unsigned char> byteData(numFloats);

	// Convert each float [0,1] → [0,255]
	for (int i = 0; i < numFloats; i++) {
		float f = std::clamp(src[i], 0.0f, 1.0f);
		byteData[i] = static_cast<unsigned char>(std::round(f * 255.0f));
	}

	// Number of bytes per row = pixels * channels
	int stride_in_bytes = width * channelCount;

	std::string filePath = "./" + name + ".png";
	if (!stbi_write_png(
		filePath.c_str(),
		width,
		height,
		channelCount,
		byteData.data(),
		stride_in_bytes
	))
	{
		throw std::runtime_error("Failed to write PNG: " + filePath);
	}
}
#include <fstream>
#include <stdexcept>
void write_raw_buffer(
	const float* src,           // your RGBA float data
	VkExtent2D         size,
	int                channelCount,  // e.g. 4
	const std::string& name           // e.g. "frame0001.raw"
) {
	uint32_t width = size.width;
	uint32_t height = size.height;
	uint32_t channels = channelCount;

	std::ofstream ofs(name, std::ios::binary);
	if (!ofs) throw std::runtime_error("Failed to open " + name);

	// (optional) write a tiny header so you can re-load more easily:
	ofs.write(reinterpret_cast<const char*>(&width), sizeof(width));
	ofs.write(reinterpret_cast<const char*>(&height), sizeof(height));
	ofs.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

	// write all floats in row-major order:
	size_t numFloats = size_t(width) * height * channels;
	ofs.write(reinterpret_cast<const char*>(src),
		numFloats * sizeof(float));

	// done
}

void VulkanEngine::draw()
{

	if (imagesBeingTransferred.size() > 0) {
		while (imagesBeingTransferred.size() > 0) {
			auto buffer = imagesBeingTransferred.front();
			uint64_t signalValue = 1;
			uint64_t expectedValue;
			VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
			waitInfo.semaphoreCount = 1;
			waitInfo.pSemaphores = &buffer.copySemaphore;
			waitInfo.pValues = &signalValue;

			// block until the GPU has signaled value ≥ 1 (infinite timeout here):
			vkWaitSemaphores(_device, &waitInfo, UINT64_MAX);


			void* srcData = nullptr;
			VkResult res = vmaMapMemory(_allocator, buffer.transferImage.allocation, &srcData);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Failed to map memory for source transfer image!");
			}

			int size = buffer.transferImage.allocation->GetSize();

			std::vector<float> hostImageData(size);

			std::memcpy(hostImageData.data(), srcData, buffer.transferImage.allocation->GetSize());

			vmaUnmapMemory(_allocator, buffer.transferImage.allocation);

			write_raw_buffer(hostImageData.data(), _windowExtent, 4, buffer.name);
			imagesBeingTransferred.erase(imagesBeingTransferred.begin());

			// read buffer out
		}
	}


	update_scene();
	// wait until the gpu has finished rendering the last frame. Timeout of 1
	// second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 10000000000));
	get_current_frame()._deletionQueue.flush();
	get_current_frame()._frameDescriptors.clear_pools(_device);
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex;




	// Handling resizing
	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);


	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;       
		return ;
	}
	if (e == VK_NOT_READY)
	{
		return;
	}

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
	_drawExtent.width= std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

	


	auto start = std::chrono::system_clock::now();


   	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	//std::cout << "Starting command buffer: " << std::addressof(cmd) << std::endl;
	// Get current frame index
	uint32_t frameIndex = _frameNumber % FRAME_OVERLAP;
	auto& currentFrame = _frames[frameIndex];

	// Calculate query indices
	currentFrame._queryStart = frameIndex * QUERIES_PER_FRAME;
	currentFrame._queryEnd = currentFrame._queryStart + 1;

	// Reset query pool segment for the current frame
	vkCmdResetQueryPool(cmd, _timestampQueryPool, currentFrame._queryStart, QUERIES_PER_FRAME);






	// Write start timestamp
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, _timestampQueryPool, currentFrame._queryStart);

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	//printf("transition depth\n");

	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	//printf("Start new deferred\n");

	std::vector<VkImage> gBufferImages = {
		_gBuffer_albedo.image,
		_gBuffer_normal.image,
		_gBuffer_metallicRoughnes.image,
		_gBuffer_Emissive.image
		};
	vkutil::transition_gbuffer_images(
		cmd,
		gBufferImages,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);
	
	WaitAll(cmd);


	//printf("first transitions\n");

	draw_deferred(cmd);
	//printf("start second transitions\n");
	//vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);



	vkutil::transition_gbuffer_images(
		cmd,
		gBufferImages,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL
	);


	//printf("second transitions\n");

	//printf("end new deferred\n");
	//printf("transition main_color_image\n");

	vkutil::transition_main_color_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	if (useRaytracing) {
		//AddCmdMarker(cmd, "Start Raytracing");
		vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		vkutil::transition_image(cmd, _colorHistory.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		WaitAll(cmd);

		_raytracingHandler.raytrace(cmd, this);
		
		WaitAll(cmd);

		_svgfHandler.Execute(cmd, this);


		//RemoveMarker(cmd);


		vkutil::transition_image(cmd, _colorHistory.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	else {
		compute_environment(cmd);

	
		vkutil::transition_shadow_map(cmd, _shadowImage->image.image, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

		draw_shadows(cmd);
	
		vkutil::transition_shadow_map(cmd, _shadowImage->image.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);


		vkutil::transition_main_color_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkutil::transition_depth(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

		draw_geometry(cmd);
		vkutil::transition_main_color_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);


	}
	WaitAll(cmd);
	TransferBuffer buffer;
	if (currentImageWriteSet.ongoing) {
		buffer = vkutil::copy_image_to_cpu_buffer(_device, _allocator, cmd, _colorHistory);
		buffer.name = std::format("image_{}", currentImageWriteSet.currentCount);


		currentImageWriteSet.currentCount++;

		// Prepare the timeline semaphore submit info.



		buffer.copySemaphore = submit_semaphores[swapchainImageIndex];

		imagesBeingTransferred.push_back(buffer);
	}
	compute_tonemapping(cmd);
	
	WaitAll(cmd);

	vkutil::transition_image(cmd, _colorHistory.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

	vkutil::transition_image_relaxed(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	//printf("time to blit\n");

	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	
	WaitAll(cmd);

	vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
	WaitAll(cmd);

	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);



	// Draw imgui into the swapchain image
	// Note: draw_imgui internally calls vkCmdBeginRendering / vkCmdEndRendering
	draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, _timestampQueryPool, currentFrame._queryEnd);

	WaitAll(cmd);


	VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, submit_semaphores[swapchainImageIndex]);
	


	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);	
	if (StartImageTransfer) {

		uint64_t signalValue = 1;

		VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = {};
		timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timelineSubmitInfo.pNext = nullptr;
		timelineSubmitInfo.waitSemaphoreValueCount = 0;
		timelineSubmitInfo.pWaitSemaphoreValues = nullptr;
		timelineSubmitInfo.signalSemaphoreValueCount = 1;
		timelineSubmitInfo.pSignalSemaphoreValues = &signalValue;
		submit.pNext = &timelineSubmitInfo;
		StartImageTransfer = false;
	}
	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));



	auto end = std::chrono::system_clock::now();

	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	uint32_t lastframeIndex = _frameNumber % FRAME_OVERLAP;

	if (_frameNumber > 0)
		retrieve_timestamp_results(lastframeIndex);


    //prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &submit_semaphores[swapchainImageIndex];
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	// Handling resizing
	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	


	//increase the number of frames drawn
	_frameNumber++;


}
template<typename T>
T roundTo(T value, int decimalPlaces)
{
	static_assert(std::is_floating_point_v<T>, "T must be float, double or long double");

	// Guard against extreme exponents that would overflow pow()
	if (std::abs(decimalPlaces) > 308)         // 308 ≈ max exponent for double
		return value;                          // nothing reasonable to do

	const T factor = std::pow(T(10), decimalPlaces);
	return std::round(value * factor) / factor;
}

void VulkanEngine::prepare_lighting_data() {
	// Point light data
	int size = sizeof(ShaderLightSource) * lightSources.size();
	_lightingDataBuffer = create_buffer(&_device, 
		&_allocator, 
		sizeof(ShaderLightSource)*lightSources.size(), 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	//add it to the deletion queue of this frame so it gets deleted once its been used
	std::vector<ShaderLightSource> shaderLightSources;
	float lightCount = lightSources.size();
	for (auto & light : lightSources) {
		ShaderLightSource shaderLight;
		shaderLight.color = light->color;
		shaderLight.direction = light->direction;
		shaderLight.intensity = light->intensity;

		shaderLight.position = light->position;
		shaderLight.type = light->type;
		shaderLight.radius = light->radius;
		shaderLight.sunAngle = light->sunAngle;
		if (light->type == LightType::Directional) {
			shaderLight.pdf = 1.0f / lightCount;
			shaderLight.intensity = light->intensity;

			float cosMax = cos(shaderLight.sunAngle / 2.0);

			float coneSolidAngle = 2.0 * glm::pi<float>() * (1.0 - cosMax);

			shaderLight.pdf = coneSolidAngle;

		}
		else if (light->type == LightType::Point){
			shaderLight.intensity = light->intensity;

			float area = light->radius * light->radius * (4.0f * glm::pi<float>());
			shaderLight.pdf = 1.0f / area / lightCount;
		}
		else {
			shaderLight.pdf = 1.0f / lightCount;
		}
		shaderLightSources.push_back(shaderLight);
	}

	// map light data to buffer
	void* mappedData = nullptr;
	VkResult result = vmaMapMemory(_allocator, _lightingDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for lighting data buffer!");
	}
	std::memcpy(mappedData, shaderLightSources.data(), sizeof(ShaderLightSource) * shaderLightSources.size());
	vmaUnmapMemory(_allocator, _lightingDataBuffer.allocation);
	
	DescriptorWriter writer3;

	uint32_t bufferSize = sizeof(ShaderLightSource) * lightSources.size();
	writer3.write_buffer(
		0,
		_lightingDataBuffer.buffer,
		bufferSize,
		0,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		lightSources.size());
	writer3.update_set(_device, _lightsourceDescriptorSet);


	//get_current_frame()._deletionQueue.push_function([=, this]() {
	//	destroy_buffer(_lightingDataBuffer);
	//	});
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	//reset counters
    stats.drawcall_count = 0;
    stats.triangle_count = 0;
    //begin clock

	
	std::vector<uint32_t> opaque_draws;
	opaque_draws.clear();
	opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

	

	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
		if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj)) {
			opaque_draws.push_back(i);
		}
	}


	// sort the opaque surfaces by material and mesh
	std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
		});
	
	glm::vec4 clearColor = glm::vec4(0.1, 0.2, 0.4, 0.97);

	//VkClearValue clearValue = { {clearColor.r, clearColor.g, clearColor.b, clearColor.a} };
	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, NULL , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	

	VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);
	
	vkCmdBeginRendering(cmd, &renderInfo);

	// vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);


	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);


	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = create_buffer(&_device, &_allocator, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	// Point light data
	AllocatedBuffer lightingDataBuffer = create_buffer(&_device, &_allocator, sizeof(PointLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	AllocatedBuffer directionalLightingDataBuffer = create_buffer(&_device, &_allocator, sizeof(DirectionalLighting), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	
	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		destroy_buffer(lightingDataBuffer);
		destroy_buffer(directionalLightingDataBuffer);
		});





	//create a descriptor set that binds that buffer and update it
	// Map memory and write the data for each buffer
	void* mappedData = nullptr;

	// Map and write GPUSceneData
	VkResult result = vmaMapMemory(_allocator, gpuSceneDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for GPU scene data buffer!");
	}
	std::memcpy(mappedData, &sceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, gpuSceneDataBuffer.allocation);

	// Map and write PointLight data
	result = vmaMapMemory(_allocator, lightingDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for lighting data buffer!");
	}
	std::memcpy(mappedData, &pointLight, sizeof(PointLight));
	vmaUnmapMemory(_allocator, lightingDataBuffer.allocation);

	// Map and write DirectionalLighting data
	result = vmaMapMemory(_allocator, directionalLightingDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for directional lighting data buffer!");
	}
	std::memcpy(mappedData, &_directionalLighting, sizeof(DirectionalLighting));
	vmaUnmapMemory(_allocator, directionalLightingDataBuffer.allocation);

	// Create descriptor sets for the buffers
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);
	VkDescriptorSet lightingDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _lightingDescriptorLayout);
	VkDescriptorSet directionalLightingDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _shadowDescriptorLayout);

	// TODO: Clean up shadow rendering
	



	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);
	writer.clear();
	writer.write_buffer(0, lightingDataBuffer.buffer, sizeof(PointLight), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, lightingDescriptor);
	writer.clear();
	// Writer shadow uniform data
	writer.write_buffer(0, directionalLightingDataBuffer.buffer, sizeof(DirectionalLighting), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, _shadowImage->image.imageView, _shadowImage->sampler, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(_device, directionalLightingDescriptor);

	
	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& r) {
		if (r.material != lastMaterial) {
			if (r.material->pipeline != lastPipeline){
				lastPipeline = r.material->pipeline;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);

				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,r.material->pipeline->layout, 0, 1,
					&globalDescriptor, 0, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,r.material->pipeline->layout, 2, 1,
					&lightingDescriptor, 0, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 3, 1,
					&directionalLightingDescriptor, 0, nullptr);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 4, 1, 
					&_textureArrayDescriptor, 0, nullptr);

				VkViewport viewport = {};
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = (float)_windowExtent.width;
				viewport.height = (float)_windowExtent.height;
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;

				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = _windowExtent.width;
				scissor.extent.height = _windowExtent.height;

				vkCmdSetScissor(cmd, 0, 1, &scissor);
			}
			// Bind material data for PBR
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
             &r.material->materialSet, 0, nullptr);
		}
		//rebind index buffer if needed
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;
		push_constants.hasTangents = r.hasTangents ? 1 : 0;
		push_constants.usePCF = usePCF ? 1 : 0;
		vkCmdPushConstants(
			cmd, 
			r.material->pipeline->layout, 
			VK_SHADER_STAGE_VERTEX_BIT | 
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		//stats
		stats.drawcall_count++;
		stats.triangle_count += r.indexCount / 3;
	};



	for (int r : opaque_draws) {
		draw(mainDrawContext.OpaqueSurfaces[r]);
	}

	/*for (auto& r : mainDrawContext.TransparentSurfaces) {
		draw(r);
	}*/



	vkCmdEndRendering(cmd);
	// we delete the draw commands now that we processed them
	mainDrawContext.OpaqueSurfaces.clear();
	mainDrawContext.TransparentSurfaces.clear();

	
}


void VulkanEngine::draw_deferred(VkCommandBuffer cmd)
{
	//reset counters
	stats.drawcall_count = 0;
	stats.triangle_count = 0;
	//begin clock


	std::vector<uint32_t> opaque_draws;
	opaque_draws.clear();
	opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());



	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
		if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj)) {
			opaque_draws.push_back(i);
		}
	}


	// sort the opaque surfaces by material and mesh
	std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
		});

	glm::vec4 clearColor = glm::vec4(0.1, 0.2, 0.4, 0.97);


	//VkClearValue VkclearValue = clearColor;
	//VkClearValue clearValue = { {clearColor.r, clearColor.g, clearColor.b, clearColor.a} };
	VkClearValue clearValue = { {0,0,0,0} };

	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;

	VkRenderingAttachmentInfo albedoAttachment = vkinit::attachment_info(_gBuffer_albedo.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, load_op);
	VkRenderingAttachmentInfo normalAttachment = vkinit::attachment_info(_gBuffer_normal.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, load_op);
	VkRenderingAttachmentInfo emissiveAttachment = vkinit::attachment_info(_gBuffer_Emissive.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, load_op);
	VkRenderingAttachmentInfo metalRougnessAttachment = vkinit::attachment_info(_gBuffer_metallicRoughnes.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, load_op);

	mColorAttachments = {
		albedoAttachment,
		normalAttachment,
		metalRougnessAttachment,
		emissiveAttachment
	};



	

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = nullptr;
	
	renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, _windowExtent };
	renderInfo.layerCount = 1;
	
	renderInfo.colorAttachmentCount = mColorAttachments.size();
	renderInfo.pColorAttachments = mColorAttachments.data();
	renderInfo.pDepthAttachment = &depthAttachment;
	renderInfo.pStencilAttachment = nullptr;


	//printf("test\n");

	vkCmdBeginRendering(cmd, &renderInfo);

	//printf("begin rendering\n");



	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);


	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = create_buffer(&_device, &_allocator, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);


	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		});





	//create a descriptor set that binds that buffer and update it
	// Map memory and write the data for each buffer
	void* mappedData = nullptr;

	// Map and write GPUSceneData
	VkResult result = vmaMapMemory(_allocator, gpuSceneDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for GPU scene data buffer!");
	}
	std::memcpy(mappedData, &sceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, gpuSceneDataBuffer.allocation);


	// Create descriptor sets for the buffers
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);


	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	int objID = 1;

	auto draw = [&](const RenderObject& r) {
		if (r.material != lastMaterial) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferredPipeline);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferredPipelineLayout, 0, 1,
				&globalDescriptor, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferredPipelineLayout, 2, 1,
				&_textureArrayDescriptor, 0, nullptr);

			VkViewport viewport = {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)_windowExtent.width;
			viewport.height = (float)_windowExtent.height;
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;

			vkCmdSetViewport(cmd, 0, 1, &viewport);

			VkRect2D scissor = {};
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = _windowExtent.width;
			scissor.extent.height = _windowExtent.height;

			vkCmdSetScissor(cmd, 0, 1, &scissor);
			
			// Bind material data for PBR
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferredPipelineLayout, 1, 1,
				&r.material->materialSet, 0, nullptr);
		}
		//rebind index buffer if needed
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;
		push_constants.hasTangents = r.hasTangents ? 1 : 0;
		push_constants.usePCF = usePCF ? 1 : 0;
		push_constants.objectId = objID++;
		vkCmdPushConstants(
			cmd,
			_deferredPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT |
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		//stats
		stats.drawcall_count++;
		stats.triangle_count += r.indexCount / 3;
		};



	for (int r : opaque_draws) {
		draw(mainDrawContext.OpaqueSurfaces[r]);
	}

	/*for (auto& r : mainDrawContext.TransparentSurfaces) {
		draw(r);
	}*/



	vkCmdEndRendering(cmd);
	// we delete the draw commands now that we processed them
	//mainDrawContext.OpaqueSurfaces.clear();
	//mainDrawContext.TransparentSurfaces.clear();


}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
		auto start = std::chrono::system_clock::now();
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_EVENT_QUIT)
                bQuit = true;

		    mainCamera->processSDLEvent(e);
			


                if (e.window.type == SDL_EVENT_WINDOW_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.type == SDL_EVENT_WINDOW_RESTORED) {
                    stop_rendering = false;
                }
			ImGui_ImplSDL3_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

		if (resize_requested) {
			resize_swapchain();
			_raytracingHandler.updateRtDescriptorSet(this);
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();


		ImGui::Begin("Main");
		//ImGui::Checkbox("Raytracing", &useRaytracing);
		//ImGui::Checkbox("Use PCF", &usePCF);
		//ImGui::Checkbox("Offline mode", &_raytracingHandler.offlineMode);
		//ImGui::InputInt("Ray budget", &_raytracingHandler.rayBudget);



		//ImGui::Text("Directional Light");
		//GPUSceneData& tmpData = sceneData;
		//ImGui::InputFloat4("Intensity", (float*)&   	tmpData.sunlightColor);
		//ImGui::InputFloat4("Direction", (float*)&    	tmpData.sunlightDirection);
		//ImGui::InputFloat4("Ambient Color", (float*)&	tmpData.ambientColor);
		
		
		ImGui::Text("Stats:");
        ImGui::Text("frametime %f ms", stats.frametime);
        ImGui::Text("draw time %f ms", stats.mesh_draw_time);
        ImGui::Text("update time %f ms", stats.scene_update_time);

		ImGui::Text("Write image sequence");
		ImGui::InputInt("Frame count", &UIImageWriteSet.count);
		ImGui::InputText("Folder name", currentString, 128);
		StartImageTransfer = ImGui::Button("Take Picture");
		ImGui::End();



		if (StartImageTransfer) {
			currentImageWriteSet = UIImageWriteSet;
			currentImageWriteSet.folder = std::string(currentString);
			currentImageWriteSet.ongoing = true;
			currentImageWriteSet.currentCount = 0;
		}

		if (currentImageWriteSet.ongoing &&
			currentImageWriteSet.count <= currentImageWriteSet.currentCount) {
			currentImageWriteSet.ongoing = false;
		}



		//make imgui calculate internal draw structures
		ImGui::Render();
		
        draw();

		//get clock again, compare with start clock
		auto end = std::chrono::system_clock::now();
		
		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;    
		cameraMoved = false;
	}
}


void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

	Uint32 count_instance_extensions;
	const char * const *instance_extensions = SDL_Vulkan_GetInstanceExtensions(&count_instance_extensions);


	auto inst_ret = builder.set_app_name("Example Vulkan Application")
				.set_engine_name("test");
	auto inst_ret2 = inst_ret.use_default_debug_messenger();
	auto inst_ret3 = inst_ret2.require_api_version(1, 3, 0).request_validation_layers(bUseValidationLayers)

		.add_validation_feature_disable(VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT)
		//.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT); // Add validation features
	auto inst_ret4 = inst_ret3.build();
	if (!inst_ret4) {
		std::cerr << "Failed to create Vulkan instance. Error: " << inst_ret4.error().message() << "\n";
		return ;
	}
    vkb::Instance vkb_inst = inst_ret4.value();

    _instance =vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;


	if (!SDL_Vulkan_CreateSurface(_window, _instance, NULL, &_surface)) {
		std::cerr << "Failed to create SDL Vulkan surface. Error: " << SDL_GetError() << "\n";
		return;
	}

	VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR localreadfeatures =
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR,
		.dynamicRenderingLocalRead = true
	};

    VkPhysicalDeviceVulkan13Features features {.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
	
    features.synchronization2 = true;
	features.privateData = true;
	

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.descriptorIndexing = true;
	features12.descriptorBindingStorageImageUpdateAfterBind = true;
	features12.descriptorBindingUniformBufferUpdateAfterBind = true;
	features12.descriptorBindingStorageBufferUpdateAfterBind = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	features12.uniformAndStorageBuffer8BitAccess = true;
	features12.uniformBufferStandardLayout = true;
	features12.scalarBlockLayout = true;
	features12.runtimeDescriptorArray = true;
	features12.shaderStorageBufferArrayNonUniformIndexing = true;
	features12.shaderSampledImageArrayNonUniformIndexing = true;
	features12.bufferDeviceAddressCaptureReplay = true;
	features12.bufferDeviceAddress = true;
	features12.hostQueryReset = true;
	features12.timelineSemaphore = true;
	
	VkPhysicalDeviceShaderClockFeaturesKHR shaderClockFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR };
	shaderClockFeatures.shaderSubgroupClock = true;

	VkPhysicalDeviceVulkan11Features features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	features11.storageBuffer16BitAccess = true;
	features11.uniformAndStorageBuffer16BitAccess = true;
	
	
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
		.rayTracingPipeline = VK_TRUE,
	};

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	accelerationStructureFeatures.accelerationStructure = true;
	




	VkPhysicalDeviceRayTracingValidationFeaturesNV rtValidationFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV };
	rtValidationFeatures.rayTracingValidation = VK_TRUE;
	
	VkPhysicalDeviceFeatures deviceFeatures{};

	deviceFeatures.shaderInt64 = true;
	deviceFeatures.shaderFloat64 = true;


	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	rayQueryFeatures.rayQuery = true;
	VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR derivativesFeatures = {};
	derivativesFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
	derivativesFeatures.computeDerivativeGroupQuads = VK_TRUE;

    //use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	

	auto tmp = selector
		.set_minimum_version(1, 3)
		.set_required_features(deviceFeatures)
		.add_required_extension("VK_KHR_acceleration_structure")
		.add_required_extension("VK_KHR_shader_relaxed_extended_instruction")
		.add_required_extension("VK_KHR_buffer_device_address")
		.add_required_extension("VK_KHR_shader_clock")
		.add_required_extension("VK_KHR_push_descriptor")
		.add_required_extension("VK_KHR_ray_query")

		.add_required_extension_features(rayQueryFeatures)
		.add_required_extension_features(localreadfeatures)
		.add_required_extension_features(accelerationStructureFeatures)
		.add_required_extension_features(shaderClockFeatures)
		.add_required_extension_features(derivativesFeatures)

		.add_required_extension_features(rtPipelineFeatures)
		.add_required_extension("VK_EXT_descriptor_buffer")
		.add_required_extension("VK_NV_ray_tracing_validation")
		.add_required_extension("VK_NV_compute_shader_derivatives")

		.add_required_extension("VK_KHR_dynamic_rendering_local_read")
		.add_required_extension_features(rtValidationFeatures)
		.add_required_extension("VK_KHR_ray_tracing_pipeline")
		.add_required_extension("VK_KHR_deferred_host_operations")
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_required_features_11(features11)
		.set_surface(_surface);


	
	auto physicalDevice = tmp.select();

	if (!physicalDevice) {
		std::cerr << "Failed to create Vulkan instance. Error: " << physicalDevice.error().message() << "\n";
		
		return;
	}

	//.add_required_extension("VK_EXT_debug_utils")
		//.add_required_extension("VK_EXT_debug_marker")
		//.add_required_extension("VK_EXT_validation_features")

	auto device = physicalDevice.value();
	//device.enable_extension_if_present(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	//device.enable_extension_if_present(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	//device.enable_extension_if_present(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

    //create the final vulkan device
	vkb::DeviceBuilder deviceBuilder{ device };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	_device = vkbDevice.device;
	_chosenGPU = device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	
	load_VK_EXTENSIONS(_instance, vkGetInstanceProcAddr, _device, vkGetDeviceProcAddr);
	
	
	
	
	// Initialize memory allocator

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.push_function([&]() {
		vmaDestroyAllocator(_allocator);
	});
}


void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	auto tmp = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();


	if (!tmp) {
			std::cerr << "Failed to create Vulkan swapchain. Error: " << tmp.error().message() << "\n";

			return;
	}
	vkb::Swapchain vkbSwapchain = tmp.value();

	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	
	submit_semaphores.resize(_swapchainImages.size());
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < _swapchainImages.size(); i++) {


		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &submit_semaphores[i]));
	}

	for (VkImage image : _swapchainImages) {
		NameImage(_device, image, "Swapchain");
	}
}
void VulkanEngine::destroy_swapchain()
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}


void VulkanEngine::create_render_buffer(AllocatedImage& image, VkImageUsageFlags usageFlags, VkImageAspectFlags aspectFlags) {
	VkImageCreateInfo rimg_info = vkinit::image_create_info(image.imageFormat, usageFlags, image.imageExtent);


	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &image.image, &image.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(image.imageFormat, image.image, aspectFlags);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &image.imageView));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, image.imageView, nullptr);
		vmaDestroyImage(_allocator, image.image, image.allocation);
	});

}



void VulkanEngine::init_swapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);


	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	// Create main color buffer
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	create_render_buffer(_drawImage, drawImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	NameImage(_device, _drawImage.image,"Draw Image");
	NameImageView(_device, _drawImage.imageView, "Draw Image View");


	VkImageUsageFlags colorHistoryUsages{};
	colorHistoryUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

	_colorHistory.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	_colorHistory.imageExtent = drawImageExtent;
	create_render_buffer(_colorHistory, drawImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	NameImage(_device, _colorHistory.image, "Color history");
	NameImageView(_device, _colorHistory.imageView, "Color history View");



	// Create depth buffer
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	depthImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;

	create_render_buffer(_depthImage, depthImageUsages, VK_IMAGE_ASPECT_DEPTH_BIT);
	NameImage(_device, _depthImage.image, "G-buffer-Depth 1 ");
	NameImageView(_device, _depthImage.imageView, "G-buffer-Depth 1 View");


	VkImageUsageFlags depthHistoryImageUsages{};
	depthHistoryImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthHistoryImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	depthHistoryImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	depthHistoryImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	_depthHistory.imageFormat = VK_FORMAT_D32_SFLOAT;

	_depthHistory.imageExtent = drawImageExtent;

	create_render_buffer(_depthHistory, depthHistoryImageUsages, VK_IMAGE_ASPECT_DEPTH_BIT);
	NameImage(_device, _depthHistory.image, "G-buffer-Depth 2 ");
	NameImageView(_device, _depthHistory.imageView, "G-buffer-Depth 2 View");


	// normal buffer
	VkImageUsageFlags normalImageUsages{};
	normalImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	normalImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	normalImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	normalImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	normalImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	_gBuffer_normal.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_gBuffer_normal.imageExtent = drawImageExtent;

	create_render_buffer(_gBuffer_normal, normalImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	NameImage(_device, _gBuffer_normal.image, "G-buffer-Normal");
	NameImageView(_device, _gBuffer_normal.imageView, "G-buffer-Normal View");


	// albedo buffer
	VkImageUsageFlags albedoImageUsages{};
	albedoImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	albedoImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	albedoImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	albedoImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	_gBuffer_albedo.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_gBuffer_albedo.imageExtent = drawImageExtent;

	create_render_buffer(_gBuffer_albedo, albedoImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	NameImage(_device, _gBuffer_albedo.image, "G-buffer-Albedo");
	NameImageView(_device, _gBuffer_albedo.imageView, "G-buffer-Albedo View");


	// material buffer
	VkImageUsageFlags materialImageUsages{};
	materialImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	materialImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	materialImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	materialImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	materialImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	_gBuffer_metallicRoughnes.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_gBuffer_metallicRoughnes.imageExtent = drawImageExtent;

	create_render_buffer(_gBuffer_metallicRoughnes, materialImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	_gBuffer_metallicRoughnes.name = "G-buffer-MetalRougness";
	NameImage(_device, _gBuffer_metallicRoughnes.image, "G-buffer-MetalRougness");
	NameImageView(_device, _gBuffer_metallicRoughnes.imageView, "G-buffer-MetalRougness View");


	// emissive buffer
	VkImageUsageFlags emissiveImageUsages{};
	emissiveImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	emissiveImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	emissiveImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	emissiveImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	emissiveImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	_gBuffer_Emissive.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	_gBuffer_Emissive.imageExtent = drawImageExtent;

	create_render_buffer(_gBuffer_Emissive, emissiveImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
	_gBuffer_Emissive.name = "G-buffer-Emissive";
	NameImage(_device, _gBuffer_Emissive.image, "G-buffer-Emissive");
	NameImageView(_device, _gBuffer_Emissive.imageView, "G-buffer-Emissive View");




}



void VulkanEngine::init_commands()
{
    //create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
	}
		
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

	_mainDeletionQueue.push_function([=]() { 
	vkDestroyCommandPool(_device, _immCommandPool, nullptr);
	});

}


void VulkanEngine::init_sync_structures()
{
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
	}
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });
}


void VulkanEngine::init_descriptors(){
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 30 }
		,
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 3}
	};

	globalDescriptorAllocator.init(_device,&_allocator, 10 , sizes);
	updatingGlobalDescriptorAllocator.init(_device, &_allocator, 10, sizes,true);
	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	{
		
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_drawImageDescriptorLayout = builder.build(_device, 
			VK_SHADER_STAGE_COMPUTE_BIT, 
			NULL, 
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, flags);
		
		_gpuSceneDataDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_lightingDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		_shadowDescriptorLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

	}
	{

		// Descriptor layout for Texture array
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, flags, 1000);
		layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, flags, 1);
		_textureArrayLayout = layoutBuilder.build(_device, 
			VK_SHADER_STAGE_VERTEX_BIT | // Might not be needed in Vertex shader. 
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
			VK_SHADER_STAGE_FRAGMENT_BIT |
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			NULL,
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

	}
	{
		VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // albedo
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // normal
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // metallic rougness
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // emissive
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // emissive
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags); // color history
		builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, flags);

		_deferredDscSetLayout = builder.build(_device,
			VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT,
			NULL,
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}
	{
		// Descriptor for tonemapping
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Color History
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Main Color

		
		_tonemappingDscSetLayout = builder.build(_device,
			VK_SHADER_STAGE_COMPUTE_BIT,
			NULL);
	}
	{
		VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, flags, 100); // 
		_lightsourceDescriptorSetLayout = builder.build(_device,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			NULL,
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	}

	_lightsourceDescriptorSet = updatingGlobalDescriptorAllocator.allocate(_device, _lightsourceDescriptorSetLayout);
	_gBufferDescriptors = updatingGlobalDescriptorAllocator.allocate(_device, _deferredDscSetLayout);
	_drawImageDescriptors = updatingGlobalDescriptorAllocator.allocate(_device,_drawImageDescriptorLayout);	
	_textureArrayDescriptor = updatingGlobalDescriptorAllocator.allocate(_device, _textureArrayLayout);
	_tonemappingImageDescriptors = globalDescriptorAllocator.allocate(_device, _tonemappingDscSetLayout);


	//make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.push_function([&]() {
		globalDescriptorAllocator.destroy_pools(_device);
		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
	});

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(_device,&_allocator, 100, frame_sizes,true);
	
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(_device);
		});
	}
}

void VulkanEngine::init_pipelines(){
	// Removed for now

	
	// init_triangle_pipeline();
	init_mesh_pipeline();
	metalRoughMaterial.build_pipelines(this);

	init_deferred_pipeline();
	init_tonemapping_pipeline();
	_shadowPipeline = std::make_unique<ShadowPipeline>();
	_shadowPipeline->build_pipelines(this);

}

void VulkanEngine::init_deferred_pipeline() {
	VkShaderModule deferredFragShader;
	if (!vkutil::load_shader_module("../shaders/spv/deferred.frag.spv", _device, &deferredFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded");
	}

	VkShaderModule deferredVertexShader;
	if (!vkutil::load_shader_module("../shaders/spv/deferred.vert.spv", _device, &deferredVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded");
	}
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


	VkDescriptorSetLayout layouts[] = {
		_gpuSceneDataDescriptorLayout,
		_gltfMaterialLayout,
		_textureArrayLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 3;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(_device, &mesh_layout_info, nullptr, &newLayout));

	_deferredPipelineLayout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(deferredVertexShader, deferredFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	//pipelineBuilder.reuse_blending_modes();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);



	std::vector<VkFormat> color_formats = {
		_gBuffer_albedo.imageFormat,
		_gBuffer_normal.imageFormat,
		_gBuffer_metallicRoughnes.imageFormat,
		_gBuffer_Emissive.imageFormat,

	};


	pipelineBuilder.set_color_attachment_format(color_formats);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	pipelineBuilder._pipelineLayout = newLayout;

	_deferredPipeline = pipelineBuilder.build_pipeline(_device);

	

	vkDestroyShaderModule(_device, deferredFragShader, nullptr);
	vkDestroyShaderModule(_device, deferredVertexShader, nullptr);
}

void VulkanEngine::init_triangle_pipeline(){
	VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module("../shaders/spv/colored_triangle.frag.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module("../shaders/spv/colored_triangle.vert.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded");
	}
	
	//build the pipeline layout that controls the inputs/outputs of the shader
	//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

	PipelineBuilder pipelineBuilder;

	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
	// connect vertex and pixel shaders
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	// set it to draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// set fill mode for polygons
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//set backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	// no multisampling
	pipelineBuilder.set_multisampling_none();
	// no blending or depth
	pipelineBuilder.disable_blending();
	pipelineBuilder.disable_depthtest();

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	//finally build the pipeline
	_trianglePipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);
	});

}





void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	// Ensure the fence is not in use
	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, VK_TRUE, 100000000000)); // 1 second timeout
	VK_CHECK(vkResetFences(_device, 1, &_immFence));

	// Reset the command buffer before re-recording
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// Execute the user-provided function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	// Prepare the command buffer for submission
	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// Submit the command buffer
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	// Wait for the GPU to finish execution
	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, VK_TRUE, 1000000000000)); // 1 second timeout
}


void VulkanEngine::init_imgui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL3_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;
	

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}


void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}



GPUMeshBuffers VulkanEngine::uploadMesh(
	std::span<uint32_t> indices,
	std::span<Vertex> vertices,
	std::span<uint32_t> raytracingIndices
) {
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
	const size_t raytracingIndexBufferSize = raytracingIndices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	// Create vertex buffer
	newSurface.vertexBuffer = create_buffer(&_device, &_allocator,
		vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_GPU_ONLY);

	// Find the address of the vertex buffer
	VkBufferDeviceAddressInfo deviceAddressInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.vertexBuffer.buffer
	};
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

	// Create index buffer
	newSurface.indexBuffer = create_buffer(&_device, &_allocator,
		indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);
	// Find the address of the vertex buffer
	VkBufferDeviceAddressInfo deviceAddressInfo1{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.indexBuffer.buffer
	};
	newSurface.IndexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo1);
	// Create raytracing index buffer only if raytracing indices are provided
	if (!raytracingIndices.empty()) {
		newSurface.indexBufferRaytracing = create_buffer(&_device, &_allocator,
			raytracingIndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VMA_MEMORY_USAGE_GPU_ONLY);
		VkBufferDeviceAddressInfo deviceAddressInfo2{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.indexBufferRaytracing.buffer
		};
		newSurface.IndexBufferAddressRaytracing = vkGetBufferDeviceAddress(_device, &deviceAddressInfo2);
	}

	// Create staging buffer
	const size_t stagingBufferSize = vertexBufferSize + indexBufferSize + raytracingIndexBufferSize;
	AllocatedBuffer staging = create_buffer(&_device, &_allocator,
		stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
		VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);

	void* data = staging.allocation->GetMappedData();

	// Copy vertex buffer data
	memcpy(data, vertices.data(), vertexBufferSize);

	// Copy index buffer data
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	// Copy raytracing index buffer data if provided
	if (!raytracingIndices.empty()) {
		memcpy((char*)data + vertexBufferSize + indexBufferSize, raytracingIndices.data(), raytracingIndexBufferSize);
	}

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);

		if (!raytracingIndices.empty()) {
			VkBufferCopy raytracingIndexCopy{ 0 };
			raytracingIndexCopy.dstOffset = 0;
			raytracingIndexCopy.srcOffset = vertexBufferSize + indexBufferSize;
			raytracingIndexCopy.size = raytracingIndexBufferSize;

			vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBufferRaytracing.buffer, 1, &raytracingIndexCopy);
		}
		});

	destroy_buffer(staging);

	return newSurface;
}


void VulkanEngine::init_mesh_pipeline(){
	VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module("../shaders/spv/tex_image.frag.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the fragment shader \n");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded \n");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module("../shaders/spv/colored_triangle_mesh.vert.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the vertex shader \n");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded \n");
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));
	
	
	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	//pipelineBuilder.disable_blending();
	pipelineBuilder.enable_blending_additive();

	// pipelineBuilder.disable_depthtest();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);


	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});
}


void VulkanEngine::init_default_data(){
	std::array<Vertex,4> rect_vertices;

	rect_vertices[0].position = {0.5,-0.5, 	0.};
	rect_vertices[1].position = {0.5,0.5, 	0.};
	rect_vertices[2].position = {-0.5,-0.5, 0.};
	rect_vertices[3].position = {-0.5,0.5, 	0.};

	rect_vertices[0].color = {0,0, 0,1};
	rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
	rect_vertices[2].color = { 1,0, 0,1 };
	rect_vertices[3].color = { 0,1, 0,1 };

	std::array<uint32_t,6> rect_indices;

	rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;

	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

	rectangle = uploadMesh(rect_indices,rect_vertices);
	
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
	_blackImage = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t default_normal_map = glm::packUnorm4x8(glm::vec4(0.5, 0.5, 1.0, 1));
	_defaultNormalMap = create_image((void*)&default_normal_map, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);



	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);


	VkExtent3D shadowExtent;
	shadowExtent.depth = 1;
	shadowExtent.height = 2048;
	shadowExtent.width = 2048;



	// Shadow image
	
	_shadowImage = std::make_unique<ShadowImage>();
	_shadowImage->prepare_image(shadowExtent, this);


	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

	_mainDeletionQueue.push_function([&](){
		vkDestroySampler(_device,_defaultSamplerNearest,nullptr);
		vkDestroySampler(_device,_defaultSamplerLinear,nullptr);

		destroy_image(_whiteImage);
		destroy_image(_greyImage);
		destroy_image(_blackImage);
		destroy_image(_errorCheckerboardImage);
	});

	

	//delete the rectangle data on engine shutdown
	_mainDeletionQueue.push_function([&](){
		destroy_buffer(rectangle.indexBuffer);
		destroy_buffer(rectangle.vertexBuffer);
	});

	GLTFMetallic_Roughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = _whiteImage;
	materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _whiteImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear;
	materialResources.normalImage = _defaultNormalMap;
	materialResources.normalSampler = _defaultSamplerLinear;
	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = create_buffer(&_device, &_allocator,sizeof(MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	MaterialConstants* sceneUniformData = (MaterialConstants*)materialConstants.allocation->GetMappedData();
	sceneUniformData->colorFactors = glm::vec4{1,1,1,1};
	sceneUniformData->metal_rough_factors = glm::vec4{1,0.5,0,0};

	_mainDeletionQueue.push_function([=, this]() {
		destroy_buffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.write_material(this,MaterialPass::MainColor,materialResources, globalDescriptorAllocator);
	defaultData.data = sceneUniformData;
	for (auto& m : testMeshes) {
		std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
		newNode->mesh = m;

		newNode->localTransform = glm::mat4{ 1.f };
		newNode->worldTransform = glm::mat4{ 1.f };

		for (auto& s : newNode->mesh->surfaces) {
			s.material = std::make_shared<GLTFMaterial>(defaultData);
		}

		loadedNodes[m->name] = std::move(newNode);
	}
	






}

void VulkanEngine::resize_swapchain()
{
	vkDeviceWaitIdle(_device);

	destroy_swapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	create_swapchain(_windowExtent.width, _windowExtent.height);
	
	resize_requested = false;
}


AllocatedImage VulkanEngine::create_cube_map_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	img_info.mipLevels = 1;
	img_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	
	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view_info.subresourceRange.layerCount = 6;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}


AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}



AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, std::string name)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(&_device, &_allocator,data_size, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(cmd, new_image.image,VkExtent2D{new_image.imageExtent.width,new_image.imageExtent.height});
        } else {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });
    destroy_buffer(uploadbuffer);
    return new_image;
}


void VulkanEngine::destroy_image(const AllocatedImage& img){
	vkDestroyImageView(_device, img.imageView, nullptr);
	vmaDestroyImage(_allocator, img.image, img.allocation);
}


void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("../shaders/spv/phong_directional.frag.spv", engine->_device, &meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("../shaders/spv/phong_directional.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);


    materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	engine->_gltfMaterialLayout = materialLayout;
	VkDescriptorSetLayout layouts[] = { 
		engine->_gpuSceneDataDescriptorLayout,
        materialLayout,
		engine->_lightingDescriptorLayout,
		engine->_shadowDescriptorLayout,
		engine->_textureArrayLayout};

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 5;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	// create the transparent variant
	pipelineBuilder.enable_blending_additive();

	pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);
	
	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VulkanEngine *engine, MaterialPass pass, MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(engine->_device, materialLayout);


	

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	writer.update_set(engine->_device, matData.materialSet);

	


	return matData;
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh->surfaces) {
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.vertexCount = s.vertexCount;
		def.firstVertex = s.startVertex;
		def.maxVertex = s.maxVertex;
		def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		def.indexBufferRaytracing = mesh->meshBuffers.indexBufferRaytracing.buffer;
		def.vertexBuffer = mesh->meshBuffers.vertexBuffer.buffer;
		def.material = &s.material->data;
		def.bounds = s.bounds;
		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
		def.indexBufferAddressRasterization = mesh->meshBuffers.IndexBufferAddress;
		def.indexBufferAddressRaytracing = mesh->meshBuffers.IndexBufferAddressRaytracing;
		def.hasTangents = s.hasTangents;
		if (s.material->data.passType == MaterialPass::Transparent) {
			ctx.TransparentSurfaces.push_back(def);
		}
		else {
			ctx.OpaqueSurfaces.push_back(def);
		}
		ctx.Both.push_back(def);
	}

	// recurse down
	Node::Draw(topMatrix, ctx);
}

void VulkanEngine::update_scene()
{
	auto start = std::chrono::system_clock::now();
	mainDrawContext.OpaqueSurfaces.clear();
	glm::mat4 topMat = glm::mat4{ 1.f };
	topMat = glm::scale(topMat, glm::vec3(1.0f));

	if (mainCamera->isMoving()) {
		cameraMoved = true;
	}


	for (auto& n : loadedScenes) {
		auto [name, node] = n;

		glm::mat4 root = topMat * node->rootTransform;
		node->Draw(root, mainDrawContext);

	}

	mainCamera->update();
	glm::vec3 jitter = glm::ballRand(0.1);

	glm::mat4 view = mainCamera->getViewMatrix(glm::vec3(0.0));
	glm::mat4 rasterizationProjection = mainCamera->getProjectionMatrix();
	glm::mat4 rayTracingProjection = mainCamera->getProjectionMatrix();


	

	sceneData.proj = rayTracingProjection;
	// Invert the Y direction on projection matrix to match OpenGL and glTF conventions
	sceneData.view = view;
	sceneData.viewproj = sceneData.proj * sceneData.view;

	

	sceneData.cameraPosition = glm::vec4(mainCamera->position, 1.0f);
	
	

	// Directly write to the mapped uniform buffer
	if (_raytracingHandler.m_uniformMappedPtr) {
		_raytracingHandler.m_uniformMappedPtr->oldViewProj = _raytracingHandler.m_uniformMappedPtr->viewProj;
		_raytracingHandler.m_uniformMappedPtr->viewProj = sceneData.viewproj;
		_raytracingHandler.m_uniformMappedPtr->viewInverse = glm::inverse(view);
		_raytracingHandler.m_uniformMappedPtr->projInverse = glm::inverse(rayTracingProjection);
		_raytracingHandler.m_uniformMappedPtr->resolution = glm::ivec2(_windowExtent.width, _windowExtent.height);
		// Update additional uniforms as needed
	}
	if (!useRaytracing) {
		directionalShadow = prepare_directional_shadow(this, this->mainDrawContext, this->sceneData.sunlightDirection);
	}

	auto end = std::chrono::system_clock::now();

	// Convert to microseconds (integer), and then back to milliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time = elapsed.count() / 1000.f;
}


void VulkanEngine::draw_shadows(VkCommandBuffer cmd) {
	//begin a render pass  connected to our draw image
	
	
	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
		opaque_draws.push_back(i);
	}

	// sort the opaque surfaces by material and mesh
	std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
	});	
	VkExtent2D viewportExtent = { _shadowImage->resolution.width, _shadowImage->resolution.height };
	
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_shadowImage->image.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	depthAttachment.clearValue = { 0.0f, 0 };
	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = nullptr;
	
	renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, viewportExtent };
	renderInfo.layerCount = 1;
	renderInfo.pDepthAttachment = &depthAttachment;
	renderInfo.pStencilAttachment = nullptr;

	vkCmdBeginRendering(cmd, &renderInfo);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = viewportExtent.width;
	viewport.height = viewportExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewportExtent.width;
	scissor.extent.height = viewportExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);
	
	AllocatedBuffer gpuShadowSceneDataBuffer = create_buffer(&_device, &_allocator, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	get_current_frame()._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuShadowSceneDataBuffer);
		});




	glm::mat4 lightProjection = directionalShadow.projectionMatrix;

	glm::mat4 lightView = directionalShadow.viewMatrix;
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	GPUSceneData shadowSceneData = {};

	shadowSceneData.proj = lightProjection;

	shadowSceneData.view = lightView;
	shadowSceneData.viewproj = lightSpaceMatrix;

	void* mappedData = nullptr;
	VkResult result = vmaMapMemory(_allocator, gpuShadowSceneDataBuffer.allocation, &mappedData);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory for GPU shadow scene data buffer!");
	}
	std::memcpy(mappedData, &shadowSceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, gpuShadowSceneDataBuffer.allocation);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuShadowSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->layout, 0, 1,
		&globalDescriptor, 0, nullptr);




	auto draw = [&](const RenderObject& r) {
		//rebind index buffer if needed
		if (r.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		push_constants.hasTangents = r.hasTangents ? 1 : 0;

		vkCmdPushConstants(cmd, 
			_shadowPipeline->layout, 
			VK_SHADER_STAGE_VERTEX_BIT | 
			VK_SHADER_STAGE_FRAGMENT_BIT, 
			0, sizeof(GPUDrawPushConstants),
			&push_constants);

		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		//stats
		stats.drawcall_count++;
		stats.triangle_count += r.indexCount / 3;
	};

	for (int r : opaque_draws) {
		draw(mainDrawContext.OpaqueSurfaces[r]);
	}

	vkCmdEndRendering(cmd);
}


void VulkanEngine::init_background_pipelines()
{

	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(GlobalUniforms);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_environmentBackgroundLayout));


	VkShaderModule backgroundShader;
	if (!vkutil::load_shader_module("../shaders/spv/environment_map_background.comp.spv", _device, &backgroundShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = backgroundShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _environmentBackgroundLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_environmentBackgroundPipeline));


	//destroy structures properly
	vkDestroyShaderModule(_device, backgroundShader, nullptr);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _environmentBackgroundLayout, nullptr);
		vkDestroyPipeline(_device, _environmentBackgroundPipeline, nullptr);
		});
}

void  VulkanEngine::compute_environment(VkCommandBuffer cmd) {
	//make a clear-color from frame number. This will flash with a 120 frame period.



	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _environmentBackgroundPipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _environmentBackgroundLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);





	vkCmdPushConstants(cmd, _environmentBackgroundLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GlobalUniforms), _raytracingHandler.m_uniformMappedPtr);


	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}


void VulkanEngine::init_tonemapping_pipeline()
{

	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_tonemappingDscSetLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ToneMappingSettings);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_tonemappingLayout));


	VkShaderModule toneMappingShader;
	if (!vkutil::load_shader_module("../shaders/spv/Tonemapping.comp.spv", _device, &toneMappingShader)) {
		fmt::print("Error when building the compute shader \n");
	}



	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = toneMappingShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _tonemappingLayout;
	computePipelineCreateInfo.stage = stageinfo;

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_tonemappingPipeline));


	//destroy structures properly
	vkDestroyShaderModule(_device, toneMappingShader, nullptr);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _tonemappingLayout, nullptr);
		vkDestroyPipeline(_device, _tonemappingPipeline, nullptr);
		});
}


void  VulkanEngine::compute_tonemapping(VkCommandBuffer cmd) {

	


	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _tonemappingPipeline);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _tonemappingDscSetLayout);

	DescriptorWriter writer;
	writer.write_image(0, _colorHistory.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, _drawImage.imageView, NULL, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(_device, globalDescriptor);


	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _tonemappingLayout, 0, 1, &globalDescriptor, 0, nullptr);


	ToneMappingSettings settings;
	settings.gamma = 2.2;




	vkCmdPushConstants(cmd, _tonemappingLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ToneMappingSettings), &settings);


	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}




void VulkanEngine::init_timestamp_queries() {
	VkQueryPoolCreateInfo queryPoolInfo{};
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = FRAME_OVERLAP * QUERIES_PER_FRAME; // Start and End per frame

	if (vkCreateQueryPool(_device, &queryPoolInfo, nullptr, &_timestampQueryPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create timestamp query pool!");
	}

	// Ensure the query pool is destroyed during cleanup
	_mainDeletionQueue.push_function([&]() {
		vkDestroyQueryPool(_device, _timestampQueryPool, nullptr);
		});
}

void VulkanEngine::retrieve_timestamp_results(uint32_t frameIndex)
{
	FrameData& currentFrame = _frames[frameIndex];
	uint32_t startQuery = currentFrame._queryStart;
	uint32_t endQuery = currentFrame._queryEnd;

	uint64_t timestamps[2] = {};

	VkResult result = vkGetQueryPoolResults(
		_device,
		_timestampQueryPool,
		startQuery,
		QUERIES_PER_FRAME,
		sizeof(timestamps),
		timestamps,
		sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
	);

	if (result == VK_SUCCESS) {
		// Retrieve timestamp period
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(_chosenGPU, &deviceProperties);
		double timestampPeriod = deviceProperties.limits.timestampPeriod; // in nanoseconds

		// Calculate the elapsed time in milliseconds
		double gpuTime = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6;

		// Update the stats
		stats.mesh_draw_time = static_cast<float>(gpuTime);
	}
	else {
		std::cerr << "Failed to retrieve timestamp results!" << std::endl;
	}
}
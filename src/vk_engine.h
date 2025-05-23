// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#ifndef VK_ENGINE_H
#define VK_ENGINE_H
#include <vk_types.h>
#include "camera.h"
#include <vulkan/vulkan_core.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <shadows.h>
#include <raytracing.h>
#include "vk_mem_alloc.h"  // No #define VMA_IMPLEMENTATION here
#include "iridescence.h"
#include <svgf.h>
// forward declarations
struct ShadowImage;
struct ShadowPipeline;
class IridescenceHandler;


struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t firstVertex;
	uint32_t vertexCount;
	uint32_t maxVertex;
	VkBuffer indexBuffer;
	VkBuffer indexBufferRaytracing;

	VkBuffer vertexBuffer;
	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
	VkDeviceAddress indexBufferAddressRaytracing;
	VkDeviceAddress indexBufferAddressRasterization;


	bool hasTangents;
};
struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
	std::vector<RenderObject> Both;
};
struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
	float reprojection_time;
	float modulate_time;
	float filter_time;
	float wavelet_time;
	float depth_normal_time;



	

};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};


struct PointLight {
	glm::vec3 position;
	float intensity;
	glm::vec4 color;
};



struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;
	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;

	// Add query indices for timestamp queries
	uint32_t _queryStart;
	uint32_t _queryEnd;
	uint32_t _depthNormalStart;
	uint32_t _depthNormalEnd;
	uint32_t _reprojectionStart;
	uint32_t _reprojectionEnd;
	uint32_t _modulateStart;
	uint32_t _modulateEnd;
	uint32_t _FilterMomentsStart;
	uint32_t _FilterMomentsEnd;
	uint32_t _waveletStart;
	uint32_t _waveletEnd;

};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
	glm::vec4 cameraPosition;
};

struct RaytracingSettings {
	int offlineMode;
	int rayBudget;
	int currentRayCount;
	uint32_t seed;
	int lightCount;
};

struct GlobalUniforms
{
	glm::mat4 viewProj;     // Camera view * projection
	glm::mat4 viewInverse;  // Camera inverse view matrix
	glm::mat4 projInverse;  // Camera inverse projection matrix
	glm::mat4 oldViewProj;
	glm::ivec2 resolution;  // Framebuffer resolution
	int clearScreen;
	int padding;
	RaytracingSettings raytracingSettings;
};

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedImage normalImage;
		VkSampler normalSampler;
		AllocatedImage emissiveImage;
		VkSampler emissiveSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
		MaterialConstants constants;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VulkanEngine* engine, MaterialPass pass, MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};


struct ImageWriteSet {
	bool ongoing;
	int count;
	int currentCount;
	std::string folder;
};



constexpr unsigned int FRAME_OVERLAP = 1;


// Forward declaration

class VulkanEngine {
public:

	bool usePCF = false;


	CubeMap _cubeMap;
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;


	// swapchain related
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;


	std::vector<VkSemaphore> submit_semaphores;
	VkExtent2D _swapchainExtent;
	DeletionQueue _mainDeletionQueue;
    VmaAllocator _allocator;
	// Framedata



	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	// Descriptor related
	DescriptorAllocatorGrowable globalDescriptorAllocator;
	DescriptorAllocatorGrowable updatingGlobalDescriptorAllocator;


	bool useSVGF = true;
	bool accumulate = false;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkDescriptorSet _gBufferDescriptors;
	VkDescriptorSetLayout _gBufferDescriptorLayout;

	bool StartImageTransfer;
	std::vector<TransferBuffer> imagesBeingTransferred;

	ImageWriteSet UIImageWriteSet;
	char currentString[128] = "tmp";

	ImageWriteSet currentImageWriteSet;

	VkPipeline _environmentBackgroundPipeline;
	VkPipelineLayout _environmentBackgroundLayout;

	VkPipeline _tonemappingPipeline;
	VkPipelineLayout _tonemappingLayout;
	VkDescriptorSetLayout _tonemappingDscSetLayout;
	VkDescriptorSet _tonemappingImageDescriptors;

	
	VkDescriptorSet _lightsourceDescriptorSet;
	VkDescriptorSetLayout _lightsourceDescriptorSetLayout;

	VkPipeline _deferredPipeline;
	VkPipelineLayout _deferredPipelineLayout;
	VkDescriptorSetLayout _deferredDscSetLayout;

	// immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	GPUMeshBuffers rectangle;

	std::vector<VkRenderingAttachmentInfo> mColorAttachments;
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _gBuffer_normal;
	AllocatedImage _gBuffer_albedo;
	AllocatedImage _gBuffer_metallicRoughnes;
	AllocatedImage _gBuffer_Emissive;




	// History buffers
	AllocatedImage _colorHistory;
	AllocatedImage _depthHistory;


	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
	AllocatedImage _defaultNormalMap;

    VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	std::unique_ptr<ShadowImage> _shadowImage;
	std::unique_ptr<ShadowPipeline> _shadowPipeline;
	VkDescriptorSetLayout _shadowDescriptorLayout;

	VkDescriptorSet _GbufferSet;
	VkDescriptorSetLayout _GbufferSetLayout;


	DirectionalLighting _directionalLighting;
	PushConstantRay _raytracePushConstant;
	VkDescriptorSet _imgui_shadow_descriptor;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;
	VkDescriptorSet environmentMapDescriptor;


	std::vector<VkImageView> textureImages;
	std::vector<VkSampler> textureSamplers;

	GPUSceneData sceneData;

	DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	VkDescriptorSetLayout _singleImageDescriptorLayout;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout; // Vertex data
	VkDescriptorSetLayout _lightingDescriptorLayout;
	VkDescriptorSetLayout _textureArrayLayout;
	VkDescriptorSetLayout _gltfMaterialLayout;
	VkDescriptorSet _textureArrayDescriptor;
	DirectionalShadow directionalShadow;
	bool resize_requested; 
	bool useRaytracing = true;
    std::shared_ptr<Camera> mainCamera;

	SVGFHandler _svgfHandler;


	VkQueryPool _timestampQueryPool;          // Query pool for timestamps
	const uint32_t QUERIES_PER_FRAME = 12;     // Start and
	
	bool _isInitialized{ false };
	int _frameNumber {0};
	int _lastSwapchainImageIndex{ -1 };
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{1280  , 720 };

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();
	void init(std::string scene_path);

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	// used for IMGUI
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex>, std::span<uint32_t> raytracingIndices = {});
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_cube_map_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage);

	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, std::string name = "Texture");
	void destroy_image(const AllocatedImage& img);
	

	void create_render_buffer(AllocatedImage& image, VkImageUsageFlags usageFlags, VkImageAspectFlags aspectFlags);

	EngineStats stats;
	PointLight pointLight;
	
	std::vector<std::shared_ptr<LightSource>> lightSources;
	AllocatedBuffer _lightingDataBuffer;

	bool cameraMoved = false;
	// Raytracing
	RaytracingHandler _raytracingHandler;

private:

	// general initialization
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_timestamp_queries();
	void init_background_pipelines();
	void retrieve_timestamp_results(uint32_t frameIndex);
	// swapchain
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void prepare_lighting_data();

	void draw_geometry(VkCommandBuffer cmd);

	void draw_shadows(VkCommandBuffer cmd);

	void draw_deferred(VkCommandBuffer cmd);

	void compute_environment(VkCommandBuffer cmd);

	void compute_tonemapping(VkCommandBuffer cmd);

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void init_descriptors();

	void init_pipelines();

	void init_imgui();

	void init_triangle_pipeline();
	void init_mesh_pipeline();
	void init_deferred_pipeline();
	void init_tonemapping_pipeline();


	void init_default_data();

	void resize_swapchain();




	void update_scene();

};

#endif // VK_ENGINE_H
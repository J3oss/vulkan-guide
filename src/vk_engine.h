#pragma once

#include <deque>
#include <vector>
#include <vk_mesh.h>
#include <vk_types.h>
#include <functional>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vk_mem_alloc.h>

#include <camera.h>
#include <vk_descriptors.h>

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 matrix;
};

struct Material {
	VkDescriptorSet textureSet{VK_NULL_HANDLE};
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct Texture {
	Image image;
	VkImageView imageView;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	glm::mat4 transform;
};

struct FrameData {
	VkFence _render_fence;
	VkSemaphore _present_semaphore, _render_semaphore;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkDescriptorSet globalDescriptorSet;
	VkDescriptorSet objectDescriptorSet;

	Buffer objectBuffer;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 viewproj;
};

struct GPUSceneData {
	glm::vec4 fogColor;
	glm::vec4 fogDistances;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection;
	glm::vec4 sunlightColor;
};

struct GPUGlobalData {
	GPUCameraData camera;
	GPUSceneData  scene;
};

struct UploadContext{
	VkFence uploadFence;
	VkCommandPool commandPool;
};

struct DeletionQueue {
	std::deque< std::function<void()> > deletors;

	void push(std::function<void()>&& func) {
		deletors.push_back(func);
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
			(*it)();

		deletors.clear();
	}
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine
{
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	VkExtent2D _windowExtent{ 1920 , 1000 };
	struct SDL_Window* _window{ nullptr };

	DeletionQueue _mainDeletionQueue;

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physical_device;
	VkDevice _logical_device;
	VkSurfaceKHR _surface;
	VkPhysicalDeviceProperties _gpuProperties;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame();

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;

	Image _depth_image;
	VkImageView _depth_image_view;
	VkFormat _depth_format;

	VkQueue _graphics_queue;
	uint32_t _graphics_family_index;

	UploadContext _uploadContext;

	VkRenderPass _render_pass;
	std::vector<VkFramebuffer> _framebuffers;

	VmaAllocator _allocator;

	DescriptorAllocator descriptoAllocator;
	DescriptorLayoutCache descriptorLayoutCache;

	VkDescriptorPool _descriptorPool;
	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorSetLayout _objectSetLayout;
	VkDescriptorSetLayout _singleTextureSetLayout;

	std::vector<RenderObject> _renderables;

	std::unordered_map <std::string,Material> _materials;
	std::unordered_map <std::string,Mesh> _meshes;

	Buffer _globalBuffer;
	Camera camera;

	void init();

	void cleanup();

	void draw();

	void run();

	Buffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	std::unordered_map<std::string, Texture> _loadedTextures;

private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_descriptors();
	void init_renderpass();
	void init_framebuffers();
	void init_sync_structures();

	void init_pipeline();

	void load_meshes();
	void upload_mesh(Mesh& mesh);

	void init_scene();
	void init_imgui();

	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout,const std::string& name);
	Material* get_material(const std::string& name);

	Mesh* get_mesh(const std::string& name);

	void draw_objects(VkCommandBuffer cmd,RenderObject* first, int count);

	size_t pad_uniform_buffer_size(size_t originalSize);

	void load_images();
};

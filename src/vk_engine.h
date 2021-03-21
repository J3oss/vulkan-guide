#pragma once

#include <deque>
#include <vector>
#include <vk_mesh.h>
#include <vk_types.h>
#include <functional>
#include <unordered_map>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 matrix;
};

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	glm::mat4 transform;
};

struct DeletionQueue {
	std::deque< std::function<void()> > deletors;

	void push(std::function<void()>&& func)
	{
		deletors.push_back(func);
	}

	void flush()
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
			(*it)();

			deletors.clear();
	}
};

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

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;

	Image _depth_image;
	VkImageView _depth_image_view;
	VkFormat _depth_format;

	VkQueue _graphics_queue;
	uint32_t _graphics_family_index;
	VkCommandPool _command_pool;
	VkCommandBuffer _main_command_buffer;

	VkRenderPass _render_pass;
	std::vector<VkFramebuffer> _framebuffers;

	VkSemaphore _present_semaphore;
	VkSemaphore _render_semaphore;
	VkFence _render_fence;

	VmaAllocator _allocator;

	std::vector<RenderObject> _renderables;

	std::unordered_map <std::string,Material> _materials;
	std::unordered_map <std::string,Mesh> _meshes;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout,const std::string& name);
	Material* get_material(const std::string& name);

	Mesh* get_mesh(const std::string& name);

	void draw_objects(VkCommandBuffer cmd,RenderObject* first, int count);
private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_renderpass();
	void init_framebuffers();
	void init_sync_structures();

	void init_pipeline();
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	void load_meshes();
	void upload_mesh(Mesh& mesh);

	void init_scene();
};

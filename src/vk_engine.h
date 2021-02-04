#pragma once

#include <vk_types.h>
#include <vector>

class VulkanEngine
{
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	VkExtent2D _windowExtent{ 1700 , 900 };
	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physical_device;
	VkDevice _logical_device;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;

	VkQueue _graphics_queue;
	uint32_t _graphics_family_index;
	VkCommandPool _command_pool;
	VkCommandBuffer _main_command_buffer;

	VkRenderPass _render_pass;
	std::vector<VkFramebuffer> _framebuffers;

	VkSemaphore _present_semaphore;
	VkSemaphore _render_semaphore;
	VkFence _render_fence;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_renderpass();
	void init_framebuffers();
	void init_sync_structures();
};

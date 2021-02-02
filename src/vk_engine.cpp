#include "vk_engine.h"
#include "iostream"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#define VK_CHECK(x)\
do { \
		VkResult err = x; \
		if(err) \
		{ std::cout <<"Detected Vulkan error: " << err << std::endl; abort(); }\
} while(0);

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	init_vulkan();
	init_swapchain();

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::init_vulkan()
{
	//instance creation
	vkb::InstanceBuilder instace_builder;
	auto instance_builder_result = instace_builder.set_app_name("vkguide")
										 														.request_validation_layers(true)
										 														.require_api_version(1, 1, 0)
										 														.use_default_debug_messenger()
										 														.build();
	vkb::Instance vkb_instance = instance_builder_result.value();

	_instance = vkb_instance.instance;
	_debug_messenger = vkb_instance.debug_messenger;

	//surface creation
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	//physical device selection
	vkb::PhysicalDeviceSelector physical_device_selector { vkb_instance };
	auto physical_selector_result = physical_device_selector.set_minimum_version(1, 1)
																													.set_surface(_surface)
																													.select();
  if (!physical_selector_result)
	{
		std::cerr << "Failed to select Vulkan Physical Device. Error: " << physical_selector_result.error().message() << "\n";
		return;
	}

  vkb::PhysicalDevice vkb_physical_device = physical_selector_result.value();

	_physical_device = vkb_physical_device.physical_device;

	//logical device
	vkb::DeviceBuilder logical_device_builder { vkb_physical_device };
	auto logical_builder_result = logical_device_builder.build();
	vkb::Device vbk_logical_device = logical_builder_result.value();

	_logical_device = vbk_logical_device.device;
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchain_builder { _physical_device, _logical_device, _surface };
	auto swapchain_builder_result = swapchain_builder.use_default_format_selection()
																														 .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
																														 .set_desired_extent(_windowExtent.width, _windowExtent.height)
																														 .build();
  vkb::Swapchain vkb_swapchain = swapchain_builder_result.value();

	_swapchain = vkb_swapchain.swapchain;
	_swapchain_images = vkb_swapchain.get_images().value();
	_swapchain_image_views = vkb_swapchain.get_image_views().value();
	_swapchain_image_format = vkb_swapchain.image_format;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		vkDestroySwapchainKHR(_logical_device, _swapchain, NULL);
		for (size_t i = 0; i < _swapchain_image_views.size(); i++)
		{
			vkDestroyImageView(_logical_device, _swapchain_image_views[i], NULL);
		}

		vkDestroyDevice(_logical_device, NULL);
		vkDestroySurfaceKHR(_instance, _surface, NULL);
		vkb::destroy_debug_utils_messenger(_instance,_debug_messenger);
		vkDestroyInstance(_instance, NULL);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	//nothing yet
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_QUIT) bQuit = true;

			if (e.type == SDL_TEXTINPUT) {
				std::cout << (char)e.key.keysym.scancode << '\n';
			}
		}

		draw();
	}
}

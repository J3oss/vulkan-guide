#include "vk_engine.h"
#include "iostream"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"
#include <math.h>

#include "vk_pipeline.h"
#include "fstream"

bool isColored = false;

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
	init_commands();
	init_renderpass();
	init_framebuffers();
	init_sync_structures();

	init_pipeline();

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
	vkb::Device vkb_logical_device = logical_builder_result.value();

	_logical_device = vkb_logical_device.device;

	//get queues
	_graphics_queue = vkb_logical_device.get_queue(vkb::QueueType::graphics).value();
	_graphics_family_index = vkb_logical_device.get_queue_index(vkb::QueueType::graphics).value();
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

void VulkanEngine::init_commands()
{
	//command pool
	VkCommandPoolCreateInfo _command_pool_info	= vkinit::command_pool_create_info(_graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(
						vkCreateCommandPool(_logical_device, &_command_pool_info, NULL, &_command_pool);
					);

	//command buffer
	VkCommandBufferAllocateInfo command_buffer_info = vkinit::command_buffer_allocate_info(_command_pool);
  VK_CHECK(
						vkAllocateCommandBuffers(_logical_device, &command_buffer_info, &_main_command_buffer);
					);
}

void VulkanEngine::init_renderpass()
{
	VkAttachmentDescription color_attachment = {	.format = _swapchain_image_format,
																								.samples = VK_SAMPLE_COUNT_1_BIT,
																								.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
																								.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
																								.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																								.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
																								.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
																								.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
																						 };
  VkAttachmentReference color_attachment_ref = {
																									.attachment = 0,
																									.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																								};

	VkSubpassDescription subpass = {
																 		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
																		.colorAttachmentCount = 1,
																		.pColorAttachments = &color_attachment_ref,
																 };

  VkRenderPassCreateInfo info = {
																	.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
																	.attachmentCount = 1,
																	.pAttachments = &color_attachment,
																	.subpassCount = 1,
																	.pSubpasses = &subpass,
																};
  VK_CHECK(vkCreateRenderPass(_logical_device, &info, NULL, &_render_pass));
}

void VulkanEngine::init_framebuffers()
{
	VkFramebufferCreateInfo info = {
																		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
																		.renderPass = _render_pass,
																		.attachmentCount = 1,
																		.width  = _windowExtent.width,
																		.height = _windowExtent.height,
																		.layers = 1,
																 };

  uint32_t image_count = _swapchain_images.size();
	_framebuffers = std::vector<VkFramebuffer> (image_count);

	for (size_t i = 0; i < image_count; i++)
	{
		info.pAttachments = &_swapchain_image_views[i];
		VK_CHECK(
							vkCreateFramebuffer(_logical_device, &info, NULL, &_framebuffers[i]);
						);
	}
}

void VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fence_create_info = {
																				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
																				.flags = VK_FENCE_CREATE_SIGNALED_BIT,
																			};
	VK_CHECK(
						vkCreateFence(_logical_device, &fence_create_info, NULL, &_render_fence)
					);

	VkSemaphoreCreateInfo sema_create_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VK_CHECK(
						vkCreateSemaphore(_logical_device, &sema_create_info, NULL, &_render_semaphore)
					);
	VK_CHECK(
						vkCreateSemaphore(_logical_device, &sema_create_info, NULL, &_present_semaphore)
					);
}

void VulkanEngine::init_pipeline()
{
	VkShaderModule red_frag_shader;
	if (!load_shader_module("../shaders/redtriangle.frag.spv", &red_frag_shader))
	std::cout << "error loading redtriangle.frag.spv" << std::endl;

	VkShaderModule red_vert_shader;
	if (!load_shader_module("../shaders/redtriangle.vert.spv", &red_vert_shader))
	std::cout << "error loading redtriangle.vert.spv" << std::endl;

	VkShaderModule colored_frag_shader;
	if (!load_shader_module("../shaders/coloredtriangle.frag.spv", &colored_frag_shader))
	std::cout << "error loading fragment shader" << std::endl;

	VkShaderModule colored_vert_shader;
	if (!load_shader_module("../shaders/coloredtriangle.vert.spv", &colored_vert_shader))
	std::cout << "error loading vertex shader" << std::endl;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(_logical_device, &pipeline_layout_info, nullptr, &_triangle_pipeline_layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.vertexInputState = vkinit::vertex_input_state_create_info();
	pipelineBuilder.inputAssemblyState = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	pipelineBuilder.viewport.x = 0.0f;
	pipelineBuilder.viewport.y = 0.0f;
	pipelineBuilder.viewport.width = (float)_windowExtent.width;
	pipelineBuilder.viewport.height = (float)_windowExtent.height;
	pipelineBuilder.viewport.minDepth = 0.0f;
	pipelineBuilder.viewport.maxDepth = 1.0f;

	pipelineBuilder.scissor.offset = { 0, 0 };
	pipelineBuilder.scissor.extent = _windowExtent;

	pipelineBuilder.rasterizerState = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipelineBuilder.multisampleState = vkinit::multisampling_state_create_info();
	pipelineBuilder.colorBlendAttachmentState = vkinit::color_blend_attachment_state();
	pipelineBuilder.pipelineLayout = _triangle_pipeline_layout;

	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, red_vert_shader));
	pipelineBuilder.shaderStages.push_back(
			vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, red_frag_shader));
	_redtriangle_pipeline = pipelineBuilder.build_pipeline(_logical_device, _render_pass);

	pipelineBuilder.shaderStages.clear();

	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, colored_vert_shader)
	);
	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colored_frag_shader)
	);
	_coloredtriangle_pipeline = pipelineBuilder.build_pipeline(_logical_device, _render_pass);
}

bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}

	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beggining
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();

	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_logical_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		return false;
	}

	*outShaderModule = shaderModule;
	return true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		vkQueueWaitIdle(_graphics_queue);

		vkDestroyPipelineLayout(_logical_device, _triangle_pipeline_layout, NULL);
		vkDestroyPipeline(_logical_device, _redtriangle_pipeline, NULL);
		vkDestroyPipeline(_logical_device, _coloredtriangle_pipeline, NULL);

		vkDestroySemaphore(_logical_device, _present_semaphore, NULL);
		vkDestroySemaphore(_logical_device, _render_semaphore, NULL);
		vkDestroyFence(_logical_device, _render_fence, NULL);

		vkDestroyRenderPass(_logical_device, _render_pass, NULL);

		vkDestroyCommandPool(_logical_device, _command_pool, NULL);

		vkDestroySwapchainKHR(_logical_device, _swapchain, NULL);
		for (size_t i = 0; i < _swapchain_image_views.size(); i++)
		{
			vkDestroyFramebuffer(_logical_device, _framebuffers[i], NULL);
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
	VK_CHECK(
						vkWaitForFences(_logical_device, 1, &_render_fence, true, 1000000000)
					);
	VK_CHECK(
						vkResetFences(_logical_device, 1, &_render_fence)
					);

	uint32_t frame_index = 0;
	VK_CHECK(
						vkAcquireNextImageKHR(_logical_device, _swapchain, 1000000000, _present_semaphore, NULL, &frame_index)
					);

  VK_CHECK(
						vkResetCommandBuffer(_main_command_buffer, 0)
					);
	VkCommandBufferBeginInfo command_buffer_begin_info = {
																													.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
																													.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
																													.pInheritanceInfo = NULL,
																												};
	VK_CHECK(vkBeginCommandBuffer(_main_command_buffer, &command_buffer_begin_info));

	VkClearValue clearValue = { };
	float flash = abs(sin(_frameNumber / 120.f));
	clearValue.color = {
												{ 0.0f, 0.0f, flash, 1.0f }
										 };

	VkRenderPassBeginInfo _render_pass_begin_info = {};
	_render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	_render_pass_begin_info.renderPass = _render_pass;
	_render_pass_begin_info.renderArea.offset.x = 0;
	_render_pass_begin_info.renderArea.offset.y = 0;
	_render_pass_begin_info.renderArea.extent = _windowExtent;
	_render_pass_begin_info.framebuffer = _framebuffers[frame_index];
	_render_pass_begin_info.clearValueCount = 1;
	_render_pass_begin_info.pClearValues = &clearValue;
	vkCmdBeginRenderPass(_main_command_buffer, &_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	//do stuff
	//do stuff
	//do stuff
	if (isColored)
		vkCmdBindPipeline(_main_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _coloredtriangle_pipeline);
	else
		vkCmdBindPipeline(_main_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _redtriangle_pipeline);

	vkCmdDraw(_main_command_buffer, 3, 1, 0, 0);
	//do stuff
	//do stuff
	//do stuff

	//finalize the render pass
	vkCmdEndRenderPass(_main_command_buffer);
	VK_CHECK(vkEndCommandBuffer(_main_command_buffer));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_present_semaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_render_semaphore;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &_main_command_buffer;

	VK_CHECK(
		vkQueueSubmit(_graphics_queue, 1, &submit, _render_fence)
	);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pSwapchains = &_swapchain;
	present_info.swapchainCount = 1;
	present_info.pWaitSemaphores = &_render_semaphore;
	present_info.waitSemaphoreCount = 1;
	present_info.pImageIndices = &frame_index;

	VK_CHECK(
		vkQueuePresentKHR(_graphics_queue, &present_info)
	);

	_frameNumber++;
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

			if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_SPACE)
				{
					std::cout << '\n' << "change pipeline" << '\n';
					isColored = !isColored;
				}
			}
		}

		draw();
	}
}

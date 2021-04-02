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

#include <vk_vma.h>

#include <glm/gtx/transform.hpp>

#include "vk_textures.h"

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
	init_descriptors();
	init_pipeline();

	load_meshes();
	load_images();

	init_scene();

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

	VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _physical_device;
  allocatorInfo.device = _logical_device;
  allocatorInfo.instance = _instance;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

	vkGetPhysicalDeviceProperties(_physical_device, &_gpuProperties);
	std::cout << "The GPU has a minimum buffer alignment of " << _gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;

	_mainDeletionQueue.push([=]() {
		vmaDestroyAllocator(_allocator);
		vkDestroyDevice(_logical_device, NULL);
		vkDestroySurfaceKHR(_instance, _surface, NULL);
		vkb::destroy_debug_utils_messenger(_instance,_debug_messenger);
		vkDestroyInstance(_instance, NULL);
	});
}

void VulkanEngine::init_swapchain()
{
	//swapchain
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

	//depth buffer
	{
		VkExtent3D depth_image_extent = {
	        _windowExtent.width,
	        _windowExtent.height,
	        1
	    };

		_depth_format = VK_FORMAT_D32_SFLOAT;

		VkImageCreateInfo depth_image_create_info = vkinit::image_create_info(_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);
		VmaAllocationCreateInfo depth_image_allocation = {};
		depth_image_allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		depth_image_allocation.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vmaCreateImage(_allocator, &depth_image_create_info, &depth_image_allocation, &_depth_image.vkimage, &_depth_image.allocation, nullptr);

		VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depth_format, _depth_image.vkimage, VK_IMAGE_ASPECT_DEPTH_BIT);
		VK_CHECK(vkCreateImageView(_logical_device, &dview_info, nullptr, &_depth_image_view));
	}

	_mainDeletionQueue.push([=]() {
		vkDestroyImageView(_logical_device, _depth_image_view, nullptr);
		vmaDestroyImage(_allocator, _depth_image.vkimage, _depth_image.allocation);

		vkDestroySwapchainKHR(_logical_device, _swapchain, NULL);
		for (size_t i = 0; i < _swapchain_image_views.size(); i++)
		{
			vkDestroyFramebuffer(_logical_device, _framebuffers[i], NULL);
			vkDestroyImageView(_logical_device, _swapchain_image_views[i], NULL);
		}
	});
}

void VulkanEngine::init_commands()
{
	//upload context command pool
	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphics_family_index);
	VK_CHECK(vkCreateCommandPool(_logical_device, &uploadCommandPoolInfo, nullptr, &_uploadContext.commandPool));
	_mainDeletionQueue.push([=]() {
		vkDestroyCommandPool(_logical_device, _uploadContext.commandPool, nullptr);
	});

	//command pools one for each frame
	VkCommandPoolCreateInfo _command_pool_info	= vkinit::command_pool_create_info(_graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	for (size_t i = 0; i < FRAME_OVERLAP; i++)
	{
		//command pool
		VK_CHECK(
			vkCreateCommandPool(_logical_device, &_command_pool_info, NULL, &_frames[i]._commandPool);
		);

		//command buffer
		VkCommandBufferAllocateInfo command_buffer_info = vkinit::command_buffer_allocate_info(_frames[i]._commandPool);
		VK_CHECK(
			vkAllocateCommandBuffers(_logical_device, &command_buffer_info, &_frames[i]._mainCommandBuffer);
		);

		_mainDeletionQueue.push([=]() {
			vkDestroyCommandPool(_logical_device, _frames[i]._commandPool, NULL);
		});
	}
}

void VulkanEngine::init_renderpass()
{
	std::vector<VkAttachmentDescription> attachments;
	attachments.push_back(vkinit::attachment_description_create(_swapchain_image_format));
	attachments.push_back(
		{
			.format = _depth_format,
    	.samples = VK_SAMPLE_COUNT_1_BIT,
  		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
  		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		});

	std::vector<VkAttachmentReference> attachment_refs;
	attachment_refs.push_back(
		{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		}
	);
	attachment_refs.push_back({
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	});

	std::vector<VkSubpassDescription> subpasses;
	subpasses.push_back(
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachment_refs.at(0),
			.pDepthStencilAttachment = &attachment_refs.at(1),
		}
	);

  VkRenderPassCreateInfo info = vkinit::render_pass_create_info(attachments, subpasses);

  VK_CHECK(vkCreateRenderPass(_logical_device, &info, NULL, &_render_pass));

	_mainDeletionQueue.push([=]() {
			vkDestroyRenderPass(_logical_device, _render_pass, NULL);
	});
}

void VulkanEngine::init_framebuffers()
{
	VkFramebufferCreateInfo info = {
																		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
																		.renderPass = _render_pass,
																		.attachmentCount = 2,
																		.width  = _windowExtent.width,
																		.height = _windowExtent.height,
																		.layers = 1,
																 };

	VkImageView attachments[2];
	attachments[1] = _depth_image_view;

  uint32_t image_count = _swapchain_images.size();
	_framebuffers = std::vector<VkFramebuffer> (image_count);

	for (size_t i = 0; i < image_count; i++)
	{
		attachments[0] = _swapchain_image_views[i];
		info.pAttachments = attachments;
		VK_CHECK(
							vkCreateFramebuffer(_logical_device, &info, NULL, &_framebuffers[i]);
						);
	}
}

void VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo uploadFenceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	VK_CHECK(
		vkCreateFence(_logical_device, &uploadFenceCreateInfo, NULL, &_uploadContext.uploadFence)
	);
	_mainDeletionQueue.push([=]() {
		vkDestroyFence(_logical_device, _uploadContext.uploadFence, NULL);
	});

	for (size_t i = 0; i < FRAME_OVERLAP; i++)
	{
		VkFenceCreateInfo fence_create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		VK_CHECK(
			vkCreateFence(_logical_device, &fence_create_info, NULL, &_frames[i]._render_fence)
		);

		VkSemaphoreCreateInfo sema_create_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(
			vkCreateSemaphore(_logical_device, &sema_create_info, NULL, &_frames[i]._render_semaphore)
		);
		VK_CHECK(
			vkCreateSemaphore(_logical_device, &sema_create_info, NULL, &_frames[i]._present_semaphore)
		);

		_mainDeletionQueue.push([=]() {
			vkDestroyFence(_logical_device, _frames[i]._render_fence, NULL);
			vkDestroySemaphore(_logical_device, _frames[i]._present_semaphore, NULL);
			vkDestroySemaphore(_logical_device, _frames[i]._render_semaphore, NULL);
		});
	}
}

void VulkanEngine::init_pipeline()
{
	VkShaderModule colored_frag_shader;
	if (!load_shader_module("../shaders/default_lit.frag.spv", &colored_frag_shader))
	std::cout << "error loading default lit shader" << std::endl;

	VkShaderModule text_frag_shader;
	if (!load_shader_module("../shaders/textured_lit.frag.spv", &text_frag_shader))
	std::cout << "error loading textured_lit lit shader" << std::endl;

	VkShaderModule mesh_vert_shader;
	if (!load_shader_module("../shaders/tri_mesh.vert.spv", &mesh_vert_shader))
	std::cout << "error loading mesh vertex shader" << std::endl;

	//mesh pipeline layout
	VkPipelineLayout _mesh_pipeline_layout;

	VkPushConstantRange pc;
	{
		pc.offset = 0;
		pc.size = sizeof(MeshPushConstants);
		pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	}
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;
	mesh_pipeline_layout_info.pPushConstantRanges = &pc;

	VkDescriptorSetLayout setLayouts[] = { _globalSetLayout, _objectSetLayout };
	mesh_pipeline_layout_info.setLayoutCount = 2;
	mesh_pipeline_layout_info.pSetLayouts = setLayouts;

	VK_CHECK(vkCreatePipelineLayout(_logical_device, &mesh_pipeline_layout_info, nullptr, &_mesh_pipeline_layout));
	_mainDeletionQueue.push([=]()
	{
		vkDestroyPipelineLayout(_logical_device, _mesh_pipeline_layout, NULL);
	});

	//textured pipeline layout
	VkPipelineLayout _textured_pipeline_layout;

	VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;
	VkDescriptorSetLayout texturedsetLayouts[] = { _globalSetLayout, _objectSetLayout, _singleTextureSetLayout };
	textured_pipeline_layout_info.setLayoutCount = 3;
	textured_pipeline_layout_info.pSetLayouts = texturedsetLayouts;

	VK_CHECK(vkCreatePipelineLayout(_logical_device, &textured_pipeline_layout_info, nullptr, &_textured_pipeline_layout));
	_mainDeletionQueue.push([=]()
	{
		vkDestroyPipelineLayout(_logical_device, _textured_pipeline_layout, NULL);
	});

	//building pipelines
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

	pipelineBuilder.depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();
	pipelineBuilder.vertexInputState.vertexBindingDescriptionCount = vertexDescription.bindings.size();
	pipelineBuilder.vertexInputState.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder.vertexInputState.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineBuilder.vertexInputState.pVertexAttributeDescriptions = vertexDescription.attributes.data();

	//building pipelines
	//mesh pipeline
	VkPipeline _mesh_pipeline;
	pipelineBuilder.pipelineLayout = _mesh_pipeline_layout;

	pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, mesh_vert_shader)
	);
	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colored_frag_shader)
	);
	_mesh_pipeline = pipelineBuilder.build_pipeline(_logical_device, _render_pass, &_mainDeletionQueue);
	create_material(_mesh_pipeline, _mesh_pipeline_layout, "defaultmesh");

	//textured pipeline
	VkPipeline _textured_pipeline;
	pipelineBuilder.pipelineLayout = _textured_pipeline_layout;

	pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, mesh_vert_shader)
	);
	pipelineBuilder.shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, text_frag_shader)
	);
	_textured_pipeline = pipelineBuilder.build_pipeline(_logical_device, _render_pass, &_mainDeletionQueue);
	create_material(_textured_pipeline, _textured_pipeline_layout, "texturedmesh");
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

	_mainDeletionQueue.push([=]()
	{
		vkDestroyShaderModule(_logical_device, shaderModule, NULL);
	});

	return true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		vkQueueWaitIdle(_graphics_queue);

		_mainDeletionQueue.flush();

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	VK_CHECK(
						vkWaitForFences(_logical_device, 1, &get_current_frame(). _render_fence, true, 1000000000)
					);
	VK_CHECK(
						vkResetFences(_logical_device, 1, &get_current_frame()._render_fence)
					);

	uint32_t frame_index = 0;
	VK_CHECK(
						vkAcquireNextImageKHR(_logical_device, _swapchain, 1000000000, get_current_frame()._present_semaphore, NULL, &frame_index)
					);

  VK_CHECK(
						vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0)
					);
	VkCommandBufferBeginInfo command_buffer_begin_info = {
																													.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
																													.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
																													.pInheritanceInfo = NULL,
																												};
	VK_CHECK(vkBeginCommandBuffer(get_current_frame()._mainCommandBuffer, &command_buffer_begin_info));

	VkClearValue clearValue = { };
	float flash = abs(sin(_frameNumber / 120.f));
	clearValue.color = {
												{ 0.0f, 0.0f, flash, 1.0f }
										 };

	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	VkRenderPassBeginInfo _render_pass_begin_info = {};
	_render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	_render_pass_begin_info.renderPass = _render_pass;
	_render_pass_begin_info.renderArea.offset.x = 0;
	_render_pass_begin_info.renderArea.offset.y = 0;
	_render_pass_begin_info.renderArea.extent = _windowExtent;
	_render_pass_begin_info.framebuffer = _framebuffers[frame_index];

	VkClearValue clearValues[] = { clearValue, depthClear };
	_render_pass_begin_info.clearValueCount = 2;
	_render_pass_begin_info.pClearValues = &clearValues[0];
	vkCmdBeginRenderPass(get_current_frame()._mainCommandBuffer, &_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	//do stuff
	//do stuff
	//do stuff
	draw_objects(get_current_frame()._mainCommandBuffer, _renderables.data(), _renderables.size());
	//do stuff
	//do stuff
	//do stuff

	//finalize the render pass
	vkCmdEndRenderPass(get_current_frame()._mainCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(get_current_frame()._mainCommandBuffer));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &get_current_frame()._present_semaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &get_current_frame()._render_semaphore;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &get_current_frame()._mainCommandBuffer;

	VK_CHECK(
		vkQueueSubmit(_graphics_queue, 1, &submit, get_current_frame()._render_fence)
	);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pSwapchains = &_swapchain;
	present_info.swapchainCount = 1;
	present_info.pWaitSemaphores = &get_current_frame()._render_semaphore;
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

void VulkanEngine::load_meshes()
{
	Mesh _triangle_mesh;
	_triangle_mesh.vertices.resize(3);

	_triangle_mesh.vertices[0].position = { 1.f, 1.f, 0.0f };
	_triangle_mesh.vertices[1].position = {-1.f, 1.f, 0.0f };
	_triangle_mesh.vertices[2].position = { 0.f,-1.f, 0.0f };

	_triangle_mesh.vertices[0].color = { 0.f, 1.f, 0.0f };
	_triangle_mesh.vertices[1].color = { 0.f, 1.f, 0.0f };
	_triangle_mesh.vertices[2].color = { 0.f, 1.f, 0.0f };

	upload_mesh(_triangle_mesh);
	_meshes["triangle"] = _triangle_mesh;

	Mesh _monkey_mesh;
	//_monkey_mesh.load_from_obj("../assets/monkey_smooth.obj");
	_monkey_mesh.assimp_load("../assets/ToyCar.glb");
	upload_mesh(_monkey_mesh);
	_meshes["monkey"] = _monkey_mesh;

	Mesh lostEmpire;
	lostEmpire.load_from_obj("../assets/lost_empire.obj");
	upload_mesh(lostEmpire);
	_meshes["empire"] = lostEmpire;
}

void VulkanEngine::upload_mesh(Mesh& mesh)
{
	const size_t bufferSize = mesh.vertices.size() * sizeof(Vertex);

	Buffer stagingBuffer;
	VkBufferCreateInfo stagingBufferCreateInfo = {};
	stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferCreateInfo.size  = bufferSize;
	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	vmaCreateBuffer(_allocator, &stagingBufferCreateInfo, &vmaallocInfo, &stagingBuffer.vkbuffer, &stagingBuffer.allocation, nullptr);

	void* data;
	vmaMapMemory(_allocator, stagingBuffer.allocation, &data);
	memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer.allocation);

	VkBufferCreateInfo bufferCreatInfo = {};
	bufferCreatInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreatInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferCreatInfo.size  = bufferSize;
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferCreatInfo, &vmaallocInfo, &mesh.verticesBuffer.vkbuffer, &mesh.verticesBuffer.allocation, nullptr));

	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer.vkbuffer, mesh.verticesBuffer.vkbuffer, 1, &copy);
	});

	_mainDeletionQueue.push([=]() {
	vmaDestroyBuffer(_allocator, mesh.verticesBuffer.vkbuffer, mesh.verticesBuffer.allocation);
	});

	vmaDestroyBuffer(_allocator, stagingBuffer.vkbuffer, stagingBuffer.allocation);
}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material material;
	material.pipeline = pipeline;
	material.pipelineLayout = layout;

	_materials[name] = material;

	return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
	auto it = _materials.find(name);

	if (it == _materials.end())
		return nullptr;

	else
		return &(*it).second;
}

Mesh* VulkanEngine::get_mesh(const std::string& name)
{
	auto it = _meshes.find(name);

	if (it == _meshes.end())
		return nullptr;

	else
		return &(*it).second;
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first, int count)
{
	int frameIndex = _frameNumber % FRAME_OVERLAP;

	glm::vec3 camPos = { 0.f,-6.f,-10.f };
	glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	GPUGlobalData globalData;
	globalData.camera.projection = projection;
	globalData.camera.view = view;
	globalData.camera.viewproj = projection * view;

	float framed = (_frameNumber / 120.f);
	globalData.scene.ambientColor = { sin(framed),0,cos(framed),1 };

	//global data
	{
		char* data;
		vmaMapMemory(_allocator, _globalBuffer.allocation, (void**) &data);

		data += pad_uniform_buffer_size(sizeof(GPUGlobalData)) * frameIndex;
		memcpy(data, &globalData, sizeof(GPUGlobalData));

		vmaUnmapMemory(_allocator, _globalBuffer.allocation);
	}

	//object data
	{
		void* objectData;
		vmaMapMemory(_allocator, get_current_frame().objectBuffer.allocation, &objectData);
		GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
		for (int i = 0; i < count; i++)
		{
			RenderObject& object = first[i];
			objectSSBO[i].modelMatrix = object.transform;
		}
		vmaUnmapMemory(_allocator, get_current_frame().objectBuffer.allocation);
	}

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < count; i++)
	{
		RenderObject& object = first[i];

		if (object.material != lastMaterial)
		{
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUGlobalData)) * frameIndex;
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
				0, 1, &get_current_frame().globalDescriptorSet, 1, &uniform_offset);
			lastMaterial = object.material;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
				1, 1, &get_current_frame().objectDescriptorSet, 0, nullptr);
		}

		glm::mat4 model = object.transform;
		glm::mat4 mesh_matrix = projection * view * model;

		MeshPushConstants constants;
		constants.matrix = object.transform;
		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		if (object.mesh != lastMesh)
		{
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->verticesBuffer.vkbuffer, &offset);
			lastMesh = object.mesh;

			if (object.material->textureSet != VK_NULL_HANDLE)
			{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
			}
		}

		vkCmdDraw(cmd, object.mesh->vertices.size(), 1, 0, i);
	}
}

void VulkanEngine::init_scene()
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
	VkSampler blockySampler;
	vkCreateSampler(_logical_device, &samplerInfo, nullptr, &blockySampler);

	Material* texturedMat=	get_material("texturedmesh");

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = nullptr;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &_singleTextureSetLayout;
	vkAllocateDescriptorSets(_logical_device, &allocInfo, &texturedMat->textureSet);

	VkDescriptorImageInfo imageBufferInfo;
	imageBufferInfo.sampler = blockySampler;
	imageBufferInfo.imageView = _loadedTextures["empire_diffuse"].imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);
	vkUpdateDescriptorSets(_logical_device, 1, &texture1, 0, nullptr);

	RenderObject monkey;
	monkey.mesh = get_mesh("monkey");
	monkey.material = get_material("defaultmesh");
	monkey.transform = glm::mat4{ 1.0f };
	_renderables.push_back(monkey);

	for (int x = -20; x <= 20; x++)
	{
		for (int y = -20; y <= 20; y++)
		{
			RenderObject triangle;
			triangle.mesh = get_mesh("triangle");
			triangle.material = get_material("defaultmesh");

			glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
			glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
			triangle.transform = translation * scale;

			_renderables.push_back(triangle);
		}
	}

	RenderObject map;
	map.mesh = get_mesh("empire");
	map.material = get_material("texturedmesh");
	map.transform = glm::translate(glm::vec3{ 5,-10,0 });
	_renderables.push_back(map);
}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

Buffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	VkBufferCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	create_info.size = allocSize;
	create_info.usage = usage;

	VmaAllocationCreateInfo vma_create_info = {};
	vma_create_info.usage = memoryUsage;

	Buffer buffer;

	VK_CHECK(
		vmaCreateBuffer(_allocator, &create_info, &vma_create_info, &buffer.vkbuffer, &buffer.allocation, nullptr);
	);

	return buffer;
}

void VulkanEngine::init_descriptors()
{
	//set layout
	{
		//global set layout
		{
			VkDescriptorSetLayoutBinding globalBufferBinding =
				vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);

			VkDescriptorSetLayoutCreateInfo setInfo = {};
			setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			setInfo.bindingCount = 1;
			setInfo.flags = 0;
			setInfo.pBindings = &globalBufferBinding;

			vkCreateDescriptorSetLayout(_logical_device, &setInfo, nullptr, &_globalSetLayout);
		}

		//object set layout
		{
			VkDescriptorSetLayoutBinding objectBinding =
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

			VkDescriptorSetLayoutBinding bindings[] = { objectBinding };

			VkDescriptorSetLayoutCreateInfo objectSet = {};
			objectSet.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			objectSet.pBindings = bindings;
			objectSet.bindingCount = 1;

			vkCreateDescriptorSetLayout(_logical_device, &objectSet, nullptr, &_objectSetLayout);
		}

		//texture set layout
		{
			VkDescriptorSetLayoutBinding textureBind =
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

			VkDescriptorSetLayoutCreateInfo setinfo = {};
			setinfo.bindingCount = 1;
			setinfo.flags = 0;
			setinfo.pNext = nullptr;
			setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			setinfo.pBindings = &textureBind;

			vkCreateDescriptorSetLayout(_logical_device, &setinfo, nullptr, &_singleTextureSetLayout);
		}
	}

	//pool
	{
		std::vector<VkDescriptorPoolSize> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 }
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = 0;
		pool_info.maxSets = 10;
		pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
		pool_info.pPoolSizes = sizes.data();

		vkCreateDescriptorPool(_logical_device, &pool_info, nullptr, &_descriptorPool);
	}

	const size_t GlobalBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUGlobalData));
	_globalBuffer = create_buffer(GlobalBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		//allocating and writing descriptors for set 1
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = _descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &_globalSetLayout;
			vkAllocateDescriptorSets(_logical_device, &allocInfo, &_frames[i].globalDescriptorSet);

			VkDescriptorBufferInfo globalInfo;
			globalInfo.buffer = _globalBuffer.vkbuffer;
			globalInfo.offset = 0;
			globalInfo.range = sizeof(GPUGlobalData);

			VkWriteDescriptorSet globalWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				_frames[i].globalDescriptorSet, &globalInfo, 0);

			vkUpdateDescriptorSets(_logical_device, 1, &globalWrite, 0, nullptr);
		}

		//allocating and writing descriptors for set 2
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = _descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &_objectSetLayout;
			vkAllocateDescriptorSets(_logical_device, &allocInfo, &_frames[i].objectDescriptorSet);

			const int MAX_OBJECTS = 10000;
			_frames[i].objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			VkDescriptorBufferInfo objectBufferInfo;
			objectBufferInfo.buffer = _frames[i].objectBuffer.vkbuffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = MAX_OBJECTS * sizeof(GPUObjectData);

			VkWriteDescriptorSet write = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				_frames[i].objectDescriptorSet, &objectBufferInfo, 0);

			vkUpdateDescriptorSets(_logical_device, 1, &write, 0, nullptr);
		}
	}
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
  VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext.commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(_logical_device, &cmdAllocInfo, &cmd));

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBeginInfo.pInheritanceInfo = NULL;
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount =1;
	submit.pCommandBuffers = &cmd;

	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit, _uploadContext.uploadFence));

	vkWaitForFences(_logical_device, 1, &_uploadContext.uploadFence, true, 9999999999);
	vkResetFences(_logical_device, 1, &_uploadContext.uploadFence);

    //clear the command pool. This will free the command buffer too
	vkResetCommandPool(_logical_device, _uploadContext.commandPool, 0);
}

void VulkanEngine::load_images()
{
	Texture lostEmpire;

	vkutil::load_image_from_file(*this, "../assets/lost_empire-RGBA.png", lostEmpire.image);

	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.vkimage, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(_logical_device, &imageinfo, nullptr, &lostEmpire.imageView);

	_loadedTextures["empire_diffuse"] = lostEmpire;
}

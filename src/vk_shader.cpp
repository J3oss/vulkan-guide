#include <vk_shader.h>
#include <vk_initializers.h>
#include <fstream>
#include <vector>
#include <iostream>

#include <spirv_reflect.h>
#include <vk_engine.h>

struct DescriptorSetLayoutData
{
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};

bool load_shader_module(VkDevice device, const char* filePath, ShaderModule* outShaderModule)
{
  //reading shader from file
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open())
  {
    std::cout << "error opening shader:" << filePath << std::endl;
    return false;
  }

  size_t fileSize = file.tellg();

  std::vector <uint32_t> buffer(fileSize / sizeof(uint32_t));

  file.seekg(0);

  file.read((char*)buffer.data(), fileSize);
  file.close();

  //creating vulkan shader module
  VkShaderModule shaderModule;

  VkShaderModuleCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = 0;
  info.flags = 0;

  info.codeSize = buffer.size() * sizeof(uint32_t);
  info.pCode = buffer.data();

  if (vkCreateShaderModule(device, &info, 0, &shaderModule) != VK_SUCCESS)
  {
    std::cout << "error creating shader module:" << filePath << std::endl;
    return false;
  }

  outShaderModule->code = std::move(buffer);
  outShaderModule->shader = shaderModule;

  return true;
}

void ShaderEffect::add_stage(ShaderModule *module, VkShaderStageFlagBits stage)
{
  ShaderStage newStage = { module,stage };
	stages.push_back(newStage);
}

void ShaderEffect::reflect_layout(VulkanEngine* engine, ReflectionOverrides* overrides, int overrideCount)
{
	std::vector<DescriptorSetLayoutData> set_layouts;

	std::vector<VkPushConstantRange> constant_ranges;

	for (auto& s : stages) {
		SpvReflectShaderModule spvmodule;
		SpvReflectResult result = spvReflectCreateShaderModule(s.shaderModule->code.size() * sizeof(uint32_t), s.shaderModule->code.data(), &spvmodule);

		uint32_t count = 0;
		result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		std::vector<SpvReflectDescriptorSet*> sets(count);
		result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, sets.data());
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		for (size_t i_set = 0; i_set < sets.size(); ++i_set)
    {
			const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);

			DescriptorSetLayoutData layout = {};// = set_layouts[i_set];

			layout.bindings.resize(refl_set.binding_count);
			for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
				const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
				VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
				layout_binding.binding = refl_binding.binding;
				layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);

				for (int ov = 0; ov < overrideCount; ov++)
				{
					if (strcmp(refl_binding.name, overrides[ov].name) == 0) {
						layout_binding.descriptorType = overrides[ov].overridenType;
					}
				}

				layout_binding.descriptorCount = 1;
				for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
					layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
				}
				layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(spvmodule.shader_stage);
			}
			layout.set_number = refl_set.set;
			layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layout.create_info.bindingCount = refl_set.binding_count;
			layout.create_info.pBindings = layout.bindings.data();

			set_layouts.push_back(layout);
		}

		//pushconstants

		result = spvReflectEnumeratePushConstants(&spvmodule, &count, NULL);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		std::vector<SpvReflectBlockVariable*> pconstants(count);
		result = spvReflectEnumeratePushConstants(&spvmodule, &count, pconstants.data());
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		if (count > 0) {
			VkPushConstantRange pcs{};
			pcs.offset = pconstants[0]->offset;
			pcs.size = pconstants[0]->size;
			pcs.stageFlags = s.stage;

			constant_ranges.push_back(pcs);
		}
	}

	std::array<DescriptorSetLayoutData,4> merged_layouts;

	for (int i = 0; i < 4; i++) {

		DescriptorSetLayoutData &ly = merged_layouts[i];

		ly.set_number = i;

		ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

		for (auto& s : set_layouts) {
			if (s.set_number == i) {
				for (auto& b : s.bindings)
				{
					ly.bindings.push_back(b);
				}
			}
		}

		ly.create_info.bindingCount = ly.bindings.size();
		ly.create_info.pBindings = ly.bindings.data();
		ly.create_info.flags = 0;
		ly.create_info.pNext = 0;

		if (ly.create_info.bindingCount > 0) {


			vkCreateDescriptorSetLayout(engine->_logical_device, &ly.create_info, nullptr, &setLayouts[i]);
		}
		else {
			setLayouts[i] = VK_NULL_HANDLE;
		}
	}

	//we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

	//setup push constants
	// VkPushConstantRange push_constant;
	// //offset 0
	// push_constant.offset = 0;
	// //size of a MeshPushConstant struct
	// push_constant.size = sizeof(MeshPushConstants);
	// //for the vertex shader
	// push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

	// mesh_pipeline_layout_info.pPushConstantRanges = constant_ranges.data();
	// mesh_pipeline_layout_info.pushConstantRangeCount = constant_ranges.size();

	mesh_pipeline_layout_info.setLayoutCount = 3;
	mesh_pipeline_layout_info.pSetLayouts = setLayouts.data();

	VkPipelineLayout meshPipLayout;
	vkCreatePipelineLayout(engine->_logical_device, &mesh_pipeline_layout_info, nullptr, &builtLayout);

}

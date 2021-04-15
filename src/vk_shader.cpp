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
  ShaderStage shader_stage = {};
  shader_stage.shaderModule = module;
  shader_stage.stage = stage;

  stages.push_back(shader_stage);
}

void ShaderEffect::reflect_layout(VkDevice device)
{
  std::vector<VkPushConstantRange> constant_ranges;
  std::vector<DescriptorSetLayoutData> set_layouts;

  for(auto shaderStage: stages)
  {
    SpvReflectShaderModule spvmodule = {};

    SpvReflectResult result = spvReflectCreateShaderModule(shaderStage.shaderModule->code.size() * sizeof(uint32_t), shaderStage.shaderModule->code.data(), &spvmodule);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
      std::cout << "error from spv" << '\n';
      return;
    }

    //descriptor sets
    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
      std::cout << "error from spv" << '\n';
      return;
    }

    std::vector<SpvReflectDescriptorSet*> spvSets(count);
    result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, spvSets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
      std::cout << "error from spv" << '\n';
      return;
    }

    for (size_t i_set = 0; i_set < count; ++i_set)
    {
        const SpvReflectDescriptorSet& refl_set = *(spvSets[i_set]);
        DescriptorSetLayoutData layout = {};

        layout.bindings.resize(refl_set.binding_count);
        for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding)
        {
          const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
          VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];

          layout_binding.binding = refl_binding.binding;
          layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
          layout_binding.descriptorCount = 1;
          for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
          {
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

    //push constants
    result = spvReflectEnumeratePushConstants(&spvmodule, &count, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
      std::cout << "error from spv" << '\n';
      return;
    }

		std::vector<SpvReflectBlockVariable*> pconstants(count);
		result = spvReflectEnumeratePushConstants(&spvmodule, &count, pconstants.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
      std::cout << "error from spv" << '\n';
      return;
    }

		if (count > 0)
    {
			VkPushConstantRange pcs{};
			pcs.offset = pconstants[0]->offset;
			pcs.size = pconstants[0]->size;
			pcs.stageFlags = shaderStage.stage;

			constant_ranges.push_back(pcs);
		}
  }

  std::array<DescriptorSetLayoutData,4> merged_layouts;

  for (int i = 0; i < 4; i++)
  {
    DescriptorSetLayoutData &ly = merged_layouts[i];
    ly.set_number = i;
    ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    for (auto& s : set_layouts)
    {
      if (s.set_number == i)
      {
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

    if (ly.create_info.bindingCount > 0)
    {
      vkCreateDescriptorSetLayout(device, &ly.create_info, nullptr, &setLayouts[i]);
    }
    else
    {
      setLayouts[i] = VK_NULL_HANDLE;
    }
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();

  VkPushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = sizeof(MeshPushConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  pipelineLayoutInfo.pPushConstantRanges = constant_ranges.data();
  pipelineLayoutInfo.pushConstantRangeCount = constant_ranges.size();
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();

	vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &builtLayout);
}

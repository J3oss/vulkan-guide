#pragma once

#include <vk_types.h>
#include <vector>
#include <array>

struct ShaderModule
{
  std::vector<uint32_t> code;
  VkShaderModule shader;
};

struct ShaderStage
{
  ShaderModule* shaderModule;
  VkShaderStageFlagBits stage;
};

struct ShaderEffect
{
public:
  
  void add_stage(ShaderModule *module, VkShaderStageFlagBits stage);
  void reflect_layout(VkDevice device);

private:
  std::vector<ShaderStage> stages;
  std::array<VkDescriptorSetLayout,4> setLayouts;
};

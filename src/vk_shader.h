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

class VulkanEngine;

struct ShaderEffect
{
public:

  struct ReflectionOverrides {
  const char* name;
  VkDescriptorType overridenType;
};

  VkPipelineLayout builtLayout;
  void add_stage(ShaderModule *module, VkShaderStageFlagBits stage);
  void reflect_layout(VulkanEngine* engine, ReflectionOverrides* overrides, int overrideCount);

private:
  std::vector<ShaderStage> stages;
  std::array<VkDescriptorSetLayout,4> setLayouts;
};

bool load_shader_module(VkDevice device, const char* filePath, ShaderModule* outShaderModule);

#pragma once

#include <vk_types.h>
#include "vk_engine.h"
#include <vector>

class PipelineBuilder
{
public:

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  VkPipelineVertexInputStateCreateInfo vertexInputState;
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineRasterizationStateCreateInfo rasterizerState;
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
  VkPipelineMultisampleStateCreateInfo multisampleState;

  VkPipelineLayout pipelineLayout;

  VkPipeline build_pipeline(VkDevice device, VkRenderPass pass,	DeletionQueue* deletor);

private:

};

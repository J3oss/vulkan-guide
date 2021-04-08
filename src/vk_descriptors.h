#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

class DescriptorAllocator
{
public:
  struct PoolSizes
  {
    std::vector<std::pair<VkDescriptorType,float>> sizes =
    {
      { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
    };
};

  void init(VkDevice Device);
  void cleanup();
  bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
  void reset();

private:
  VkDevice device;
  std::vector<VkDescriptorPool> usedPools;
  std::vector<VkDescriptorPool> freePools;

  PoolSizes descriptorSizes;

  VkDescriptorPool grab_pool();
  VkDescriptorPool currentPool{ VK_NULL_HANDLE };

  VkDescriptorPool createPool(VkDevice device, const PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags);
};

class DescriptorLayoutCache
{
public:
  struct DescriptorLayoutInfo
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bool operator==(const DescriptorLayoutInfo& other) const;
		size_t hash() const;
  };

  void init(VkDevice Device);
  void cleanup();
  VkDescriptorSetLayout create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

private:
  VkDevice device;
  struct DescriptorLayoutHash
  {
			std::size_t operator()(const DescriptorLayoutInfo& k) const{
				return k.hash();
			}
	};

	std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
};

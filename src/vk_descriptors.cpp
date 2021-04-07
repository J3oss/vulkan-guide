#include <vk_descriptors.h>

void DescriptorAllocator::init(VkDevice Device)
{
	device = Device;
}

void DescriptorAllocator::cleanup()
{
  for(auto p : freePools)
    vkDestroyDescriptorPool(device, p, nullptr);

  for(auto p : usedPools)
    vkDestroyDescriptorPool(device, p, nullptr);
}

VkDescriptorPool DescriptorAllocator::createPool(VkDevice device, const PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags)
{
  std::vector<VkDescriptorPoolSize> sizes;
  for(auto ps : poolSizes.sizes)
    sizes.push_back( { ps.first, uint32_t(ps.second * count) } );

  VkDescriptorPoolCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.pNext = VK_NULL_HANDLE;
  info.flags = flags;
  info.maxSets = count;
  info.poolSizeCount = (uint32_t)sizes.size();
  info.pPoolSizes = sizes.data();

  VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool);

	return descriptorPool;
}

VkDescriptorPool DescriptorAllocator::grab_pool()
{
  VkDescriptorPool pool;

  if (freePools.size())
  {
    pool = freePools.back();
    freePools.pop_back();
  }

  else
  {
    pool = createPool(device, descriptorSizes, 1000, 0);
  }

  return pool;
}

bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
  if (currentPool == VK_NULL_HANDLE)
  {
    currentPool == grab_pool();
    usedPools.push_back(currentPool);
  }

  VkDescriptorSetAllocateInfo info;
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.pNext = nullptr;
  info.descriptorPool = currentPool;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &layout;

  VkResult allocResult = vkAllocateDescriptorSets(device, &info, set);
  bool needReallocate = false;

  switch (allocResult)
  {
  	case VK_SUCCESS:
  		return true;

  	case VK_ERROR_FRAGMENTED_POOL:
  	case VK_ERROR_OUT_OF_POOL_MEMORY:
  		needReallocate = true;
  		break;

  	default:
  	   return false;
  }

  if (needReallocate)
  {
		currentPool = grab_pool();
		usedPools.push_back(currentPool);

		allocResult = vkAllocateDescriptorSets(device, &info, set);

		//if it still fails then we have big issues
		if (allocResult == VK_SUCCESS)
    {
			return true;
		}
	}

  return false;
}

void DescriptorAllocator::reset()
{
  for(auto p : usedPools)
  {
    vkResetDescriptorPool(device, p, 0);
  }

  freePools = usedPools;
	usedPools.clear();

	currentPool = VK_NULL_HANDLE;
}

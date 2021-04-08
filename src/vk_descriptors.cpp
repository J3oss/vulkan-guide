#include <vk_descriptors.h>
#include <algorithm>

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




void DescriptorLayoutCache::init(VkDevice Device)
{
  device = Device;
}

void DescriptorLayoutCache::cleanup()
{
  for(auto layout : layoutCache)
  {
    vkDestroyDescriptorSetLayout(device, layout.second, nullptr);
  }
}

VkDescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
	DescriptorLayoutInfo layoutinfo;
	bool isSorted = true;
	int lastBinding = -1;

	layoutinfo.bindings.reserve(info->bindingCount);
	for (size_t i = 0; i < info->bindingCount; i++)
	{
		layoutinfo.bindings.push_back(info->pBindings[i]);

		if (lastBinding > info->pBindings[i].binding)
			isSorted = false;

		lastBinding = info->pBindings[i].binding;
	}

	if (!isSorted)
	{
		std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(),
					[](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b ){
						return a.binding < b.binding;
					});
	}

	auto it = layoutCache.find(layoutinfo);
	if (it != layoutCache.end())
	{
		return (*it).second;
	}
	else
	{
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout( device, info, nullptr, &layout);
		layoutCache[layoutinfo] = layout;
		return layout;
	}
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
	if (other.bindings.size() != bindings.size()){
		return false;
	}
	else {
		//compare each of the bindings is the same. Bindings are sorted so they will match
		for (int i = 0; i < bindings.size(); i++) {
			if (other.bindings[i].binding != bindings[i].binding){
				return false;
			}
			if (other.bindings[i].descriptorType != bindings[i].descriptorType){
				return false;
			}
			if (other.bindings[i].descriptorCount != bindings[i].descriptorCount){
				return false;
			}
			if (other.bindings[i].stageFlags != bindings[i].stageFlags){
				return false;
			}
		}
		return true;
	}
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const{
		using std::size_t;
		using std::hash;

		size_t result = hash<size_t>()(bindings.size());

		for (const VkDescriptorSetLayoutBinding& b : bindings)
		{
			//pack the binding data into a single int64. Not fully correct but it's ok
			size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

			//shuffle the packed binding data and xor it with the main hash
			result ^= hash<size_t>()(binding_hash);
		}

		return result;
	}




DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
{
	DescriptorBuilder builder;

	builder.cache = layoutCache;
	builder.alloc = allocator;
	return builder;
}

DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	//create the descriptor binding for the layout
	VkDescriptorSetLayoutBinding newBinding{};
	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	//create the descriptor write
	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = bufferInfo;
	newWrite.dstBinding = binding;

	writes.push_back(newWrite);
	return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	//create the descriptor binding for the layout
	VkDescriptorSetLayoutBinding newBinding{};
	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	//create the descriptor write
	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = imageInfo;
	newWrite.dstBinding = binding;

	writes.push_back(newWrite);
	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout){
	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = bindings.size();

	layout = cache->create_descriptor_layout(&layoutInfo);

	//allocate descriptor
	bool success = alloc->allocate(&set, layout);
	if (!success) { return false; };

	//write descriptor
	for (VkWriteDescriptorSet& w : writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(alloc->device, writes.size(), writes.data(), 0, nullptr);

	return true;
}

// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/vulkan/GrManagerImpl.h>
#include <anki/gr/GrManager.h>

#include <anki/gr/Pipeline.h>
#include <anki/gr/vulkan/CommandBufferImpl.h>
#include <anki/gr/CommandBuffer.h>

#include <anki/util/HashMap.h>
#include <anki/util/Hash.h>
#include <anki/core/Config.h>
#include <glslang/Public/ShaderLang.h>

namespace anki
{

#define ANKI_GR_MANAGER_DEBUG_MEMMORY ANKI_DEBUG

//==============================================================================
// GrManagerImpl::CompatibleRenderPassHashMap                                  =
//==============================================================================

class RenderPassKey
{
public:
	Array<PixelFormat, MAX_COLOR_ATTACHMENTS> m_colorAttachments;
	PixelFormat m_depthStencilAttachment;

	RenderPassKey()
	{
		// Zero because we compute hashes
		memset(this, 0, sizeof(*this));
	}

	RenderPassKey& operator=(const RenderPassKey& b) = default;
};

class RenderPassHasher
{
public:
	U64 operator()(const RenderPassKey& b) const
	{
		return computeHash(&b, sizeof(b));
	}
};

class RenderPassCompare
{
public:
	Bool operator()(const RenderPassKey& a, const RenderPassKey& b) const
	{
		for(U i = 0; i < a.m_colorAttachments.getSize(); ++i)
		{
			if(a.m_colorAttachments[i] != b.m_colorAttachments[i])
			{
				return false;
			}
		}

		return a.m_depthStencilAttachment == b.m_depthStencilAttachment;
	}
};

class GrManagerImpl::CompatibleRenderPassHashMap
{
public:
	Mutex m_mtx;
	HashMap<RenderPassKey, VkRenderPass, RenderPassHasher, RenderPassCompare>
		m_hashmap;
};

//==============================================================================
// GrManagerImpl                                                               =
//==============================================================================

//==============================================================================
GrManagerImpl::~GrManagerImpl()
{
	// FIRST THING: wait for the GPU
	if(m_queue)
	{
		LockGuard<Mutex> lock(m_queueSubmitMtx);
		vkQueueWaitIdle(m_queue);
		m_queue = VK_NULL_HANDLE;
	}

	// SECOND THING: The destroy everything that has a reference to GrObjects.
	for(auto& x : m_perFrame)
	{
		if(x.m_imageView)
		{
			vkDestroyImageView(m_device, x.m_imageView, nullptr);
			x.m_imageView = VK_NULL_HANDLE;
		}

		x.m_presentFence.reset(nullptr);
		x.m_acquireSemaphore.reset(nullptr);
		x.m_renderSemaphore.reset(nullptr);

		x.m_cmdbsSubmitted.destroy(getAllocator());
	}

	m_perThread.destroy(getAllocator());

	// THIRD THING: Continue with the rest
	if(m_renderPasses)
	{
		auto it = m_renderPasses->m_hashmap.getBegin();
		auto end = m_renderPasses->m_hashmap.getEnd();
		while(it != end)
		{
			VkRenderPass pass = (*it);
			vkDestroyRenderPass(m_device, pass, nullptr);
			++it;
		}

		m_renderPasses->m_hashmap.destroy(getAllocator());
		getAllocator().deleteInstance(m_renderPasses);
	}

	if(m_globalPipelineLayout)
	{
		vkDestroyPipelineLayout(m_device, m_globalPipelineLayout, nullptr);
	}

	if(m_globalDescriptorPool)
	{
		vkDestroyDescriptorPool(m_device, m_globalDescriptorPool, nullptr);
	}

	if(m_globalDescriptorSetLayout)
	{
		vkDestroyDescriptorSetLayout(
			m_device, m_globalDescriptorSetLayout, nullptr);
	}

	m_transientMem.destroy();
	m_gpuMemAllocs.destroy(getAllocator());

	m_semaphores.destroy(); // Destroy before fences
	m_fences.destroy();

	if(m_swapchain)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}

	if(m_device)
	{
		vkDestroyDevice(m_device, nullptr);
	}

	if(m_surface)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

	if(m_instance)
	{
		vkDestroyInstance(m_instance, nullptr);
	}
}

//==============================================================================
GrAllocator<U8> GrManagerImpl::getAllocator() const
{
	return m_manager->getAllocator();
}

//==============================================================================
Error GrManagerImpl::init(const GrManagerInitInfo& init)
{
	Error err = initInternal(init);
	if(err)
	{
		ANKI_LOGE("Vulkan initialization failed");
		return ErrorCode::FUNCTION_FAILED;
	}

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initInternal(const GrManagerInitInfo& init)
{
	ANKI_LOGI("Initializing Vulkan backend");
	ANKI_CHECK(initInstance(init));
	ANKI_CHECK(initSurface(init));
	ANKI_CHECK(initDevice(init));
	vkGetDeviceQueue(m_device, m_queueIdx, 0, &m_queue);
	ANKI_CHECK(initSwapchain(init));

	ANKI_CHECK(initMemory(*init.m_config));
	ANKI_CHECK(initGlobalDsetLayout());
	ANKI_CHECK(initGlobalDsetPool());
	ANKI_CHECK(initGlobalPplineLayout());

	m_renderPasses = getAllocator().newInstance<CompatibleRenderPassHashMap>();

	for(PerFrame& f : m_perFrame)
	{
		resetFrame(f);
	}

	glslang::InitializeProcess();
	m_fences.init(getAllocator(), m_device);
	m_semaphores.init(getAllocator(), m_device);

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initInstance(const GrManagerInitInfo& init)
{
	// Create the instance
	//
	static Array<const char*, 8> LAYERS = {{"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_GOOGLE_unique_objects",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_standard_validation"}};

	static Array<const char*, 2> EXTENSIONS = {
		{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME}};

	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "unamed";
	app.applicationVersion = 1;
	app.pEngineName = "AnKi 3D Engine";
	app.engineVersion = (ANKI_VERSION_MAJOR << 1) | ANKI_VERSION_MINOR;
	app.apiVersion = VK_MAKE_VERSION(1, 0, 3);

	VkInstanceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &app;

	if(init.m_config->getNumber("debugContext"))
	{
		ANKI_LOGI("VK: Will enable debug layers");
		ci.enabledLayerCount = LAYERS.getSize();
		ci.ppEnabledLayerNames = &LAYERS[0];
	}

	ci.enabledExtensionCount = EXTENSIONS.getSize();
	ci.ppEnabledExtensionNames = &EXTENSIONS[0];

#if ANKI_GR_MANAGER_DEBUG_MEMMORY
	VkAllocationCallbacks allocCbs = {};
	VkAllocationCallbacks* pallocCbs = &allocCbs;
	allocCbs.pUserData = this;
	allocCbs.pfnAllocation = allocateCallback;
	allocCbs.pfnReallocation = reallocateCallback;
	allocCbs.pfnFree = freeCallback;
#else
	VkAllocationCallbacks* pallocCbs = nullptr;
#endif

	ANKI_VK_CHECK(vkCreateInstance(&ci, pallocCbs, &m_instance));

	// Create the physical device
	//
	uint32_t count = 0;
	ANKI_VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, nullptr));
	ANKI_LOGI("VK: Number of physical devices: %u", count);
	if(count < 1)
	{
		ANKI_LOGE("Wrong number of physical devices");
		return ErrorCode::FUNCTION_FAILED;
	}

	count = 1;
	ANKI_VK_CHECK(
		vkEnumeratePhysicalDevices(m_instance, &count, &m_physicalDevice));

	vkGetPhysicalDeviceProperties(m_physicalDevice, &m_devProps);

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initDevice(const GrManagerInitInfo& init)
{
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);
	ANKI_LOGI("VK: Number of queue families: %u\n", count);

	DynamicArrayAuto<VkQueueFamilyProperties> queueInfos(getAllocator());
	queueInfos.create(count);
	vkGetPhysicalDeviceQueueFamilyProperties(
		m_physicalDevice, &count, &queueInfos[0]);

	uint32_t desiredFamilyIdx = MAX_U32;
	const VkQueueFlags DESITED_QUEUE_FLAGS =
		VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
	for(U i = 0; i < count; ++i)
	{
		if((queueInfos[i].queueFlags & (DESITED_QUEUE_FLAGS))
			== DESITED_QUEUE_FLAGS)
		{
			VkBool32 supportsPresent = false;
			ANKI_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
				m_physicalDevice, i, m_surface, &supportsPresent));

			if(supportsPresent)
			{
				desiredFamilyIdx = i;
				break;
			}
		}
	}

	if(desiredFamilyIdx == MAX_U32)
	{
		ANKI_LOGE("Couldn't find a queue family with graphics+compute+present."
				  "The assumption was wrong. The code needs rework");
		return ErrorCode::FUNCTION_FAILED;
	}

	m_queueIdx = desiredFamilyIdx;

	F32 priority = 1.0;
	VkDeviceQueueCreateInfo q = {};
	q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	q.queueFamilyIndex = desiredFamilyIdx;
	q.queueCount = 1;
	q.pQueuePriorities = &priority;

	static Array<const char*, 1> DEV_EXTENSIONS = {
		{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};

	VkDeviceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.queueCreateInfoCount = 1;
	ci.pQueueCreateInfos = &q;
	ci.enabledExtensionCount = DEV_EXTENSIONS.getSize();
	ci.ppEnabledExtensionNames = &DEV_EXTENSIONS[0];

	ANKI_VK_CHECK(vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device));

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initSwapchain(const GrManagerInitInfo& init)
{
	VkSurfaceCapabilitiesKHR surfaceProperties;
	ANKI_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		m_physicalDevice, m_surface, &surfaceProperties));

	if(surfaceProperties.currentExtent.width == MAX_U32
		|| surfaceProperties.currentExtent.height == MAX_U32)
	{
		ANKI_LOGE("Wrong surface size");
		return ErrorCode::FUNCTION_FAILED;
	}
	m_surfaceWidth = surfaceProperties.currentExtent.width;
	m_surfaceHeight = surfaceProperties.currentExtent.height;

	uint32_t formatCount;
	ANKI_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
		m_physicalDevice, m_surface, &formatCount, nullptr));

	DynamicArrayAuto<VkSurfaceFormatKHR> formats(getAllocator());
	formats.create(formatCount);
	ANKI_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
		m_physicalDevice, m_surface, &formatCount, &formats[0]));

	VkColorSpaceKHR colorspace = VK_COLOR_SPACE_MAX_ENUM_KHR;
	while(formatCount--)
	{
		if(formats[formatCount].format == VK_FORMAT_B8G8R8A8_SRGB)
		{
			m_surfaceFormat = formats[formatCount].format;
			colorspace = formats[formatCount].colorSpace;
			break;
		}
	}

	if(m_surfaceFormat == VK_FORMAT_UNDEFINED)
	{
		ANKI_LOGE("Surface format not found");
		return ErrorCode::FUNCTION_FAILED;
	}

	VkSwapchainCreateInfoKHR ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = m_surface;
	ci.minImageCount = MAX_FRAMES_IN_FLIGHT;
	ci.imageFormat = m_surfaceFormat;
	ci.imageColorSpace = colorspace;
	ci.imageExtent = surfaceProperties.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.queueFamilyIndexCount = 1;
	ci.pQueueFamilyIndices = &m_queueIdx;
	ci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	ci.clipped = false;
	ci.oldSwapchain = VK_NULL_HANDLE;

	ANKI_VK_CHECK(vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain));

	// Get images
	uint32_t count = 0;
	ANKI_VK_CHECK(
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr));
	if(count != MAX_FRAMES_IN_FLIGHT)
	{
		ANKI_LOGE("Requested a swapchain with %u images but got one with %u",
			MAX_FRAMES_IN_FLIGHT,
			count);
		return ErrorCode::FUNCTION_FAILED;
	}

	ANKI_LOGI("VK: Swapchain images count %u", count);

	Array<VkImage, MAX_FRAMES_IN_FLIGHT> images;
	ANKI_VK_CHECK(
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, &images[0]));
	for(U i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_perFrame[i].m_image = images[i];
		ANKI_ASSERT(images[i]);
	}

	// Create img views
	for(U i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		PerFrame& perFrame = m_perFrame[i];

		VkImageViewCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.flags = 0;
		ci.image = perFrame.m_image;
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = m_surfaceFormat;
		ci.components = {VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A};
		ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ci.subresourceRange.baseMipLevel = 0;
		ci.subresourceRange.levelCount = 1;
		ci.subresourceRange.baseArrayLayer = 0;
		ci.subresourceRange.layerCount = 1;

		ANKI_VK_CHECK(
			vkCreateImageView(m_device, &ci, nullptr, &perFrame.m_imageView));
	}

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initGlobalDsetLayout()
{
	VkDescriptorSetLayoutCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	const U BINDING_COUNT = MAX_TEXTURE_BINDINGS + MAX_UNIFORM_BUFFER_BINDINGS
		+ MAX_STORAGE_BUFFER_BINDINGS;
	ci.bindingCount = BINDING_COUNT;

	Array<VkDescriptorSetLayoutBinding, BINDING_COUNT> bindings;
	memset(&bindings[0], 0, sizeof(bindings));
	ci.pBindings = &bindings[0];

	U count = 0;

	// Combined image samplers
	for(U i = 0; i < MAX_TEXTURE_BINDINGS; ++i)
	{
		VkDescriptorSetLayoutBinding& binding = bindings[count];
		binding.binding = count;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_ALL;

		++count;
	}

	// Uniform buffers
	for(U i = 0; i < MAX_UNIFORM_BUFFER_BINDINGS; ++i)
	{
		VkDescriptorSetLayoutBinding& binding = bindings[count];
		binding.binding = count;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_ALL;

		++count;
	}

	// Storage buffers
	for(U i = 0; i < MAX_STORAGE_BUFFER_BINDINGS; ++i)
	{
		VkDescriptorSetLayoutBinding& binding = bindings[count];
		binding.binding = count;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_ALL;

		++count;
	}

	ANKI_ASSERT(count == BINDING_COUNT);

	ANKI_VK_CHECK(vkCreateDescriptorSetLayout(
		m_device, &ci, nullptr, &m_globalDescriptorSetLayout));

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initGlobalDsetPool()
{
	Array<VkDescriptorPoolSize, 3> pools = {{}};
	pools[0] = VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		MAX_TEXTURE_BINDINGS * MAX_RESOURCE_GROUPS};
	pools[1] = VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		MAX_UNIFORM_BUFFER_BINDINGS * MAX_RESOURCE_GROUPS};
	pools[2] = VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
		MAX_STORAGE_BUFFER_BINDINGS * MAX_RESOURCE_GROUPS};

	VkDescriptorPoolCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	ci.maxSets = MAX_RESOURCE_GROUPS;
	ci.poolSizeCount = pools.getSize();
	ci.pPoolSizes = &pools[0];

	ANKI_VK_CHECK(vkCreateDescriptorPool(
		m_device, &ci, nullptr, &m_globalDescriptorPool));

	return ErrorCode::NONE;
}

//==============================================================================
Error GrManagerImpl::initGlobalPplineLayout()
{
	Array<VkDescriptorSetLayout, MAX_BOUND_RESOURCE_GROUPS> sets = {
		{m_globalDescriptorSetLayout, m_globalDescriptorSetLayout}};

	VkPipelineLayoutCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	ci.pNext = nullptr;
	ci.flags = 0;
	ci.setLayoutCount = MAX_BOUND_RESOURCE_GROUPS;
	ci.pSetLayouts = &sets[0];
	ci.pushConstantRangeCount = 0;
	ci.pPushConstantRanges = nullptr;

	ANKI_VK_CHECK(vkCreatePipelineLayout(
		m_device, &ci, nullptr, &m_globalPipelineLayout));

	return ErrorCode::NONE;
}

//==============================================================================
VkRenderPass GrManagerImpl::getOrCreateCompatibleRenderPass(
	const PipelineInitInfo& init)
{
	VkRenderPass out = VK_NULL_HANDLE;

	// Create the key
	RenderPassKey key;
	for(U i = 0; i < init.m_color.m_attachmentCount; ++i)
	{
		key.m_colorAttachments[i] = init.m_color.m_attachments[i].m_format;
	}
	key.m_depthStencilAttachment = init.m_depthStencil.m_format;

	// Lock
	LockGuard<Mutex> lock(m_renderPasses->m_mtx);

	auto it = m_renderPasses->m_hashmap.find(key);
	if(it != m_renderPasses->m_hashmap.getEnd())
	{
		// Found the key
		out = *it;
	}
	else
	{
		// Not found, create one
		VkRenderPassCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		Array<VkAttachmentDescription, MAX_COLOR_ATTACHMENTS + 1>
			attachmentDescriptions;
		memset(&attachmentDescriptions[0], 0, sizeof(attachmentDescriptions));

		Array<VkAttachmentReference, MAX_COLOR_ATTACHMENTS> references;
		memset(&references[0], 0, sizeof(references));

		for(U i = 0; i < init.m_color.m_attachmentCount; ++i)
		{
			// We only care about samples and format
			VkAttachmentDescription& desc = attachmentDescriptions[i];
			desc.format = (!init.m_color.m_drawsToDefaultFramebuffer)
				? convertFormat(init.m_color.m_attachments[i].m_format)
				: m_surfaceFormat;
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

			references[i].attachment = i;
			references[i].layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		ci.attachmentCount = init.m_color.m_attachmentCount;

		Bool hasDepthStencil =
			init.m_depthStencil.m_format.m_components != ComponentFormat::NONE;
		VkAttachmentReference dsReference = {0, VK_IMAGE_LAYOUT_GENERAL};
		if(hasDepthStencil)
		{
			VkAttachmentDescription& desc =
				attachmentDescriptions[ci.attachmentCount];
			desc.format = convertFormat(init.m_depthStencil.m_format);
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			desc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

			dsReference.attachment = ci.attachmentCount;
			dsReference.layout = VK_IMAGE_LAYOUT_GENERAL;

			++ci.attachmentCount;
		}

		VkSubpassDescription desc = {};
		desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		desc.colorAttachmentCount = init.m_color.m_attachmentCount;
		desc.pColorAttachments =
			(init.m_color.m_attachmentCount) ? &references[0] : nullptr;
		desc.pDepthStencilAttachment =
			(hasDepthStencil) ? &dsReference : nullptr;

		ANKI_ASSERT(ci.attachmentCount);
		ci.pAttachments = &attachmentDescriptions[0];
		ci.subpassCount = 1;
		ci.pSubpasses = &desc;

		VkRenderPass rpass;
		ANKI_VK_CHECKF(vkCreateRenderPass(m_device, &ci, nullptr, &rpass));

		m_renderPasses->m_hashmap.pushBack(getAllocator(), key, rpass);
		out = rpass;
	}

	ANKI_ASSERT(out);
	return out;
}

//==============================================================================
Error GrManagerImpl::initMemory(const ConfigSet& cfg)
{
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

	// Create the high level allocators
	m_gpuMemAllocs.create(getAllocator(), m_memoryProperties.memoryTypeCount);
	U idx = 0;
	for(GpuMemoryAllocator& alloc : m_gpuMemAllocs)
	{
		alloc.init(getAllocator(), m_device, idx++);
	}

	// Transient mem
	ANKI_CHECK(m_transientMem.init(cfg));

	return ErrorCode::NONE;
}

//==============================================================================
U GrManagerImpl::findMemoryType(U resourceMemTypeBits,
	VkMemoryPropertyFlags preferFlags,
	VkMemoryPropertyFlags avoidFlags) const
{
	U preferedHigh = MAX_U32;
	U preferedMed = MAX_U32;

	// Iterate all mem types
	for(U i = 0; i < m_memoryProperties.memoryTypeCount; i++)
	{
		if(resourceMemTypeBits & (1u << i))
		{
			VkMemoryPropertyFlags flags =
				m_memoryProperties.memoryTypes[i].propertyFlags;

			if((flags & preferFlags) == preferFlags)
			{
				preferedMed = i;

				if((flags & avoidFlags) != avoidFlags)
				{
					preferedHigh = i;
				}
			}
		}
	}

	if(preferedHigh < MAX_U32)
	{
		return preferedHigh;
	}
	else
	{
		return preferedMed;
	}
}

//==============================================================================
void* GrManagerImpl::allocateCallback(void* userData,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	ANKI_ASSERT(userData);
	GrManagerImpl* self = static_cast<GrManagerImpl*>(userData);
	return self->getAllocator().getMemoryPool().allocate(size, alignment);
}

//==============================================================================
void* GrManagerImpl::reallocateCallback(void* userData,
	void* original,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	ANKI_ASSERT(0 && "TODO");
	return nullptr;
}

//==============================================================================
void GrManagerImpl::freeCallback(void* userData, void* ptr)
{
	if(ptr)
	{
		ANKI_ASSERT(userData);
		GrManagerImpl* self = static_cast<GrManagerImpl*>(userData);
		self->getAllocator().getMemoryPool().free(ptr);
	}
}

//==============================================================================
void GrManagerImpl::beginFrame()
{
	PerFrame& frame = m_perFrame[m_frame % MAX_FRAMES_IN_FLIGHT];

	// Create a semaphore
	frame.m_acquireSemaphore = newSemaphore();

	// Get new image
	uint32_t imageIdx;
	ANKI_VK_CHECKF(vkAcquireNextImageKHR(m_device,
		m_swapchain,
		UINT64_MAX,
		frame.m_acquireSemaphore->getHandle(),
		0,
		&imageIdx));
	ANKI_ASSERT(
		imageIdx == (m_frame % MAX_FRAMES_IN_FLIGHT) && "Wrong assumption");
}

//==============================================================================
void GrManagerImpl::endFrame()
{
	PerFrame& frame = m_perFrame[m_frame % MAX_FRAMES_IN_FLIGHT];

	// Wait for the fence of N-2 frame
	U waitFrameIdx = (m_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	PerFrame& waitFrame = m_perFrame[waitFrameIdx];
	if(waitFrame.m_presentFence)
	{
		waitFrame.m_presentFence->wait();
	}

	resetFrame(waitFrame);

	if(!frame.m_renderSemaphore)
	{
		ANKI_LOGW("Nobody draw to the default framebuffer");
	}

	// Present
	uint32_t imageIdx = m_frame % MAX_FRAMES_IN_FLIGHT;
	VkPresentInfoKHR present = {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = (frame.m_renderSemaphore) ? 1 : 0;
	present.pWaitSemaphores = (frame.m_renderSemaphore)
		? &frame.m_renderSemaphore->getHandle()
		: nullptr;
	present.swapchainCount = 1;
	present.pSwapchains = &m_swapchain;
	present.pImageIndices = &imageIdx;

	{
		LockGuard<Mutex> lock(m_queueSubmitMtx);
		ANKI_VK_CHECKF(vkQueuePresentKHR(m_queue, &present));
	}

	// Finalize
	++m_frame;

#if ANKI_ASSERTIONS
	ANKI_ASSERT(m_cmdbWithIndicationThatIsFirstSubmitted
		&& m_cmdbWithIndicationThatIsLastSubmitted
		&& "Forgot to set some command buffer flags");
	m_cmdbWithIndicationThatIsFirstSubmitted = false;
	m_cmdbWithIndicationThatIsLastSubmitted = false;
#endif
}

//==============================================================================
void GrManagerImpl::resetFrame(PerFrame& frame)
{
	frame.m_presentFence.reset(nullptr);
	frame.m_acquireSemaphore.reset(nullptr);
	frame.m_renderSemaphore.reset(nullptr);

	frame.m_cmdbsSubmitted.destroy(getAllocator());
}

//==============================================================================
GrManagerImpl::PerThread& GrManagerImpl::getPerThreadCache(Thread::Id tid)
{
	PerThread* thread = nullptr;
	LockGuard<SpinLock> lock(m_perThreadMtx);

	// Find or create a record
	auto it = m_perThread.find(tid);
	if(it != m_perThread.getEnd())
	{
		thread = &(*it);
	}
	else
	{
		m_perThread.emplaceBack(getAllocator(), tid);
		it = m_perThread.find(tid);
		thread = &(*it);
	}

	return *thread;
}

//==============================================================================
VkCommandBuffer GrManagerImpl::newCommandBuffer(
	Thread::Id tid, Bool secondLevel)
{
	// Get the per thread cache
	PerThread& thread = getPerThreadCache(tid);

	// Try initialize the recycler
	if(ANKI_UNLIKELY(!thread.m_cmdbs.isCreated()))
	{
		Error err = thread.m_cmdbs.init(getAllocator(), m_device, m_queueIdx);
		if(err)
		{
			ANKI_LOGF("Cannot recover");
		}
	}

	return thread.m_cmdbs.newCommandBuffer(secondLevel);
}

//==============================================================================
void GrManagerImpl::deleteCommandBuffer(
	VkCommandBuffer cmdb, Bool secondLevel, Thread::Id tid)
{
	// Get the per thread cache
	PerThread& thread = getPerThreadCache(tid);

	thread.m_cmdbs.deleteCommandBuffer(cmdb, secondLevel);
}

//==============================================================================
void GrManagerImpl::flushCommandBuffer(CommandBufferPtr cmdb,
	SemaphorePtr signalSemaphore,
	WeakArray<SemaphorePtr> waitSemaphores,
	WeakArray<VkPipelineStageFlags> waitPplineStages)
{
	CommandBufferImpl& impl = cmdb->getImplementation();
	VkCommandBuffer handle = impl.getHandle();

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	FencePtr fence = newFence();
	PerFrame& frame = m_perFrame[m_frame % MAX_FRAMES_IN_FLIGHT];

	if(signalSemaphore)
	{
		submit.pSignalSemaphores = &signalSemaphore->getHandle();
		submit.signalSemaphoreCount = 1;
		signalSemaphore->setFence(fence);
	}

	Array<VkSemaphore, 16> allWaitSemaphores;
	Array<VkPipelineStageFlags, 16> allWaitPplineStages;
	for(U i = 0; i < waitSemaphores.getSize(); ++i)
	{
		ANKI_ASSERT(waitSemaphores[i]);
		allWaitSemaphores[submit.waitSemaphoreCount] =
			waitSemaphores[i]->getHandle();
		allWaitPplineStages[submit.waitSemaphoreCount] = waitPplineStages[i];
		++submit.waitSemaphoreCount;
	}

	// Do some special stuff for the last command buffer
	if(impl.renderedToDefaultFramebuffer())
	{
		allWaitSemaphores[submit.waitSemaphoreCount] =
			frame.m_acquireSemaphore->getHandle();
		allWaitPplineStages[submit.waitSemaphoreCount] =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		++submit.waitSemaphoreCount;

		// Create the semaphore to signal
		ANKI_ASSERT(!frame.m_renderSemaphore
			&& "Only one begin/end render pass is allowed with the default fb");
		frame.m_renderSemaphore = newSemaphore();

		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &frame.m_renderSemaphore->getHandle();

		frame.m_presentFence = fence;
	}

	submit.pWaitSemaphores = &allWaitSemaphores[0];
	submit.pWaitDstStageMask = &allWaitPplineStages[0];

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &handle;

	// Lock to submit
	LockGuard<Mutex> lock(m_queueSubmitMtx);

	frame.m_cmdbsSubmitted.pushBack(getAllocator(), cmdb);

#if ANKI_ASSERTIONS
	if(impl.isTheFirstFramebufferOfTheFrame())
	{
		ANKI_ASSERT(m_cmdbWithIndicationThatIsFirstSubmitted == false);
		m_cmdbWithIndicationThatIsFirstSubmitted = true;
	}

	if(impl.isTheLastFramebufferOfTheFrame())
	{
		ANKI_ASSERT(m_cmdbWithIndicationThatIsLastSubmitted == false);
		m_cmdbWithIndicationThatIsLastSubmitted = true;
	}
#endif

	ANKI_VK_CHECKF(vkQueueSubmit(m_queue, 1, &submit, fence->getHandle()));
}

} // end namespace anki

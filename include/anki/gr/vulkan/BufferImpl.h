// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/vulkan/VulkanObject.h>
#include <anki/gr/vulkan/GpuMemoryAllocator.h>

namespace anki
{

/// @addtogroup vulkan
/// @{

/// Buffer implementation
class BufferImpl : public VulkanObject
{
public:
	BufferImpl(GrManager* manager)
		: VulkanObject(manager)
	{
	}

	~BufferImpl();

	ANKI_USE_RESULT Error init(
		PtrSize size, BufferUsageBit usage, BufferMapAccessBit access);

	ANKI_USE_RESULT void* map(
		PtrSize offset, PtrSize range, BufferMapAccessBit access);

	void unmap()
	{
		ANKI_ASSERT(isCreated());
		ANKI_ASSERT(m_mapped);

#if ANKI_ASSERTIONS
		m_mapped = false;
#endif
	}

	VkBuffer getHandle() const
	{
		ANKI_ASSERT(isCreated());
		return m_handle;
	}

private:
	VkBuffer m_handle = VK_NULL_HANDLE;
	GpuMemoryAllocationHandle m_memHandle;
	U32 m_memIdx = 0;
	BufferMapAccessBit m_access = BufferMapAccessBit::NONE;
	U32 m_size = 0;

#if ANKI_ASSERTIONS
	Bool8 m_mapped = false;
#endif

	Bool isCreated() const
	{
		return m_handle != VK_NULL_HANDLE;
	}
};
/// @}

} // end namespace anki

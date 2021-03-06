// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/vulkan/VulkanObject.h>
#include <anki/util/BitSet.h>

namespace anki
{

/// @addtogroup vulkan
/// @{

/// Resource group implementation.
class ResourceGroupImpl : public VulkanObject
{
public:
	ResourceGroupImpl(GrManager* manager)
		: VulkanObject(manager)
	{
	}

	~ResourceGroupImpl();

	ANKI_USE_RESULT Error init(const ResourceGroupInitInfo& init);

	Bool hasDescriptorSet() const
	{
		return m_handle != VK_NULL_HANDLE;
	}

	VkDescriptorSet getHandle() const
	{
		ANKI_ASSERT(m_handle);
		return m_handle;
	}

	void getVertexBindingInfo(const VkBuffer*& buffers,
		const VkDeviceSize*& offsets,
		U& bindingCount) const
	{
		buffers = &m_vertBuffs[0];
		offsets = &m_offsets[0];
		bindingCount = m_bindingCount;
	}

	void setupDynamicOffsets(
		const TransientMemoryInfo* dynInfo, U32 dynOffsets[]) const;

	/// Get index buffer info.
	/// @return false if there is no index buffer.
	Bool getIndexBufferInfo(
		VkBuffer& buff, VkDeviceSize& offset, VkIndexType& idxType) const
	{
		if(m_indexBuffHandle)
		{
			buff = m_indexBuffHandle;
			offset = m_indexBufferOffset;
			idxType = m_indexType;
			return true;
		}

		return false;
	}

private:
	VkDescriptorSet m_handle = VK_NULL_HANDLE;
	Array<VkBuffer, MAX_VERTEX_ATTRIBUTES> m_vertBuffs = {{}};
	Array<VkDeviceSize, MAX_VERTEX_ATTRIBUTES> m_offsets = {{}};
	U32 m_bindingCount = 0;

	// For dynamic binding
	U8 m_uniBindingCount = 0;
	BitSet<MAX_UNIFORM_BUFFER_BINDINGS + MAX_STORAGE_BUFFER_BINDINGS>
		m_dynamicBuffersMask = {false};

	// Index info
	VkBuffer m_indexBuffHandle = VK_NULL_HANDLE;
	U32 m_indexBufferOffset = MAX_U32;
	VkIndexType m_indexType = VK_INDEX_TYPE_MAX_ENUM;

	/// Holds the references to the resources. Used to release the references
	/// gracefully
	DynamicArray<GrObjectPtr<GrObject>> m_refs;

	static U calcRefCount(const ResourceGroupInitInfo& init, Bool& hasUploaded);
};
/// @}

} // end namespace anki

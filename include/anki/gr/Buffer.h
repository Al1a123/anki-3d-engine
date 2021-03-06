// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/GrObject.h>

namespace anki
{

/// @addtogroup graphics
/// @{

/// GPU buffer.
class Buffer : public GrObject
{
public:
	static const GrObjectType CLASS_TYPE = GrObjectType::BUFFER;

	/// Construct.
	Buffer(GrManager* manager, U64 hash = 0);

	/// Destroy.
	~Buffer();

	/// Access the implementation.
	BufferImpl& getImplementation()
	{
		return *m_impl;
	}

	/// Allocate the buffer.
	void init(PtrSize size, BufferUsageBit usage, BufferMapAccessBit access);

	/// Map the buffer.
	void* map(PtrSize offset, PtrSize range, BufferMapAccessBit access);

	/// Unmap the buffer.
	void unmap();

private:
	UniquePtr<BufferImpl> m_impl;
};
/// @}

} // end namespace anki

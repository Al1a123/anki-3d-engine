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

/// Sampler initializer.
class SamplerInitInfo
{
public:
	F32 m_minLod = -1000.0;
	F32 m_maxLod = 1000.0;
	SamplingFilter m_minMagFilter = SamplingFilter::NEAREST;
	SamplingFilter m_mipmapFilter = SamplingFilter::BASE;
	CompareOperation m_compareOperation = CompareOperation::ALWAYS;
	I8 m_anisotropyLevel = 0;
	Bool8 m_repeat = true; ///< Repeat or clamp.

	U64 computeHash() const
	{
		return anki::computeHash(
			this, offsetof(SamplerInitInfo, m_repeat) + sizeof(m_repeat));
	}
};

static_assert(offsetof(SamplerInitInfo, m_repeat) == 12,
	"Class needs to be tightly packed since we hash it");

/// Texture initializer.
class TextureInitInfo
{
public:
	TextureType m_type = TextureType::_2D;
	TextureUsageBit m_usage = TextureUsageBit::NONE;
	TextureUsageBit m_initialUsage = TextureUsageBit::NONE;
	U32 m_width = 0;
	U32 m_height = 0;
	U32 m_depth = 0; //< Relevant only for 3D textures.
	U32 m_layerCount = 0; ///< Relevant only for texture arrays.
	U8 m_mipmapsCount = 0;

	PixelFormat m_format;
	U8 m_samples = 1;

	SamplerInitInfo m_sampling;

	/// Check the validity of the structure.
	Bool isValid() const
	{
#define ANKI_CHECK_VAL_VALIDITY(x)                                             \
	do                                                                         \
	{                                                                          \
		if(!(x))                                                               \
		{                                                                      \
			return false;                                                      \
		}                                                                      \
	} while(0)

		// TODO: ANKI_CHECK_VAL_VALIDITY(m_usage != TextureUsageBit::NONE);
		ANKI_CHECK_VAL_VALIDITY(m_mipmapsCount > 0);
		ANKI_CHECK_VAL_VALIDITY(m_width > 0);
		ANKI_CHECK_VAL_VALIDITY(m_height > 0);
		switch(m_type)
		{
		case TextureType::_2D:
			ANKI_CHECK_VAL_VALIDITY(m_depth == 1);
			ANKI_CHECK_VAL_VALIDITY(m_layerCount == 1);
			break;
		case TextureType::CUBE:
			ANKI_CHECK_VAL_VALIDITY(m_depth == 1);
			ANKI_CHECK_VAL_VALIDITY(m_layerCount == 1);
			break;
		case TextureType::_3D:
			ANKI_CHECK_VAL_VALIDITY(m_depth > 0);
			ANKI_CHECK_VAL_VALIDITY(m_layerCount == 1);
			break;
		case TextureType::_2D_ARRAY:
		case TextureType::CUBE_ARRAY:
			ANKI_CHECK_VAL_VALIDITY(m_depth == 1);
			ANKI_CHECK_VAL_VALIDITY(m_layerCount > 0);
			break;
		default:
			ANKI_CHECK_VAL_VALIDITY(0);
		};

		return true;
#undef ANKI_CHECK_VAL_VALIDITY
	}
};

/// GPU texture
class Texture : public GrObject
{
public:
	static const GrObjectType CLASS_TYPE = GrObjectType::TEXTURE;

	/// Construct.
	Texture(GrManager* manager, U64 hash = 0);

	/// Destroy.
	~Texture();

	/// Access the implementation.
	TextureImpl& getImplementation()
	{
		return *m_impl;
	}

	/// Create it.
	void init(const TextureInitInfo& init);

private:
	UniquePtr<TextureImpl> m_impl;
};
/// @}

} // end namespace anki

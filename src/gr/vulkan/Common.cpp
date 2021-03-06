// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/vulkan/Common.h>

namespace anki
{

//==============================================================================
void computeBarrierInfo(TextureUsageBit before,
	TextureUsageBit after,
	Bool isDepthStencil,
	U level,
	U levelCount,
	VkPipelineStageFlags& srcStages,
	VkAccessFlags& srcAccesses,
	VkPipelineStageFlags& dstStages,
	VkAccessFlags& dstAccesses)
{
	ANKI_ASSERT(level < levelCount && levelCount > 0);
	srcStages = 0;
	srcAccesses = 0;
	dstStages = 0;
	dstAccesses = 0;
	Bool lastLevel = level == levelCount - 1;

	//
	// Before
	//
	if((before & TextureUsageBit::FRAGMENT_SHADER_SAMPLED)
		!= TextureUsageBit::NONE)
	{
		srcStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		srcAccesses |= VK_ACCESS_SHADER_READ_BIT;
	}

	if((before & TextureUsageBit::COMPUTE_SHADER_SAMPLED)
		!= TextureUsageBit::NONE)
	{
		srcStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		srcAccesses |= VK_ACCESS_SHADER_READ_BIT;
	}

	if((before & TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ)
		!= TextureUsageBit::NONE)
	{
		if(isDepthStencil)
		{
			srcStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			srcAccesses |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		else
		{
			srcStages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			srcAccesses |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		}
	}

	if((before & TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE)
		!= TextureUsageBit::NONE)
	{
		srcStages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

		if(isDepthStencil)
		{
			srcAccesses |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		else
		{
			srcAccesses |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
	}

	if((before & TextureUsageBit::GENERATE_MIPMAPS) != TextureUsageBit::NONE)
	{
		srcStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if(!lastLevel)
		{
			srcAccesses |= VK_ACCESS_TRANSFER_READ_BIT;
		}
		else
		{
			srcAccesses |= VK_ACCESS_TRANSFER_WRITE_BIT;
		}
	}

	if((before & TextureUsageBit::UPLOAD) != TextureUsageBit::NONE)
	{
		srcStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		srcAccesses |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	if(srcStages == 0)
	{
		srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}

	//
	// After
	//
	if((after & TextureUsageBit::FRAGMENT_SHADER_SAMPLED)
		!= TextureUsageBit::NONE)
	{
		dstStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dstAccesses |= VK_ACCESS_SHADER_READ_BIT;
	}

	if((after & TextureUsageBit::COMPUTE_SHADER_SAMPLED)
		!= TextureUsageBit::NONE)
	{
		dstStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		dstAccesses |= VK_ACCESS_SHADER_READ_BIT;
	}

	if((after & TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ)
		!= TextureUsageBit::NONE)
	{
		if(isDepthStencil)
		{
			dstStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dstAccesses |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		else
		{
			dstStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dstAccesses |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		}
	}

	if((after & TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE)
		!= TextureUsageBit::NONE)
	{
		if(isDepthStencil)
		{
			dstStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dstAccesses |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		else
		{
			dstStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dstAccesses |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
	}

	if((after & TextureUsageBit::GENERATE_MIPMAPS) != TextureUsageBit::NONE)
	{
		dstStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if(level == 0)
		{
			dstAccesses |= VK_ACCESS_TRANSFER_READ_BIT;
		}
		else
		{
			ANKI_ASSERT(0 && "This will happen in generateMipmaps");
		}
	}

	if((after & TextureUsageBit::UPLOAD) != TextureUsageBit::NONE)
	{
		dstStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstAccesses |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	ANKI_ASSERT(dstStages);
}

//==============================================================================
VkImageLayout computeLayout(
	TextureUsageBit usage, Bool isDepthStencil, U level, U levelCount)
{
	ANKI_ASSERT(level < levelCount && levelCount > 0);

	VkImageLayout out = VK_IMAGE_LAYOUT_MAX_ENUM;
	Bool lastLevel = level == levelCount - 1;

	if(usage == TextureUsageBit::NONE)
	{
		out = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	else if(isDepthStencil)
	{
		if(usage == TextureUsageBit::FRAGMENT_SHADER_SAMPLED
			|| usage == TextureUsageBit::COMPUTE_SHADER_SAMPLED)
		{
			out = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else if(usage == TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE
			|| usage == TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE)
		{
			out = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else if(usage == (TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ
							 | TextureUsageBit::FRAGMENT_SHADER_SAMPLED))
		{
			out = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else if(usage == TextureUsageBit::GENERATE_MIPMAPS)
		{
			if(!lastLevel)
			{
				out = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			else
			{
				out = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
		}
	}
	else
	{
		if(usage == TextureUsageBit::FRAGMENT_SHADER_SAMPLED
			|| usage == TextureUsageBit::COMPUTE_SHADER_SAMPLED)
		{
			out = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else if(usage == TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE
			|| usage == TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE)
		{
			out = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		else if(usage == TextureUsageBit::GENERATE_MIPMAPS)
		{
			if(!lastLevel)
			{
				out = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			else
			{
				out = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
		}
		else if(usage == TextureUsageBit::UPLOAD)
		{
			out = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}
	}

	ANKI_ASSERT(out != VK_IMAGE_LAYOUT_MAX_ENUM);
	return out;
}

//==============================================================================
VkCompareOp convertCompareOp(CompareOperation ak)
{
	VkCompareOp out = VK_COMPARE_OP_NEVER;
	switch(ak)
	{
	case CompareOperation::ALWAYS:
		out = VK_COMPARE_OP_ALWAYS;
		break;
	case CompareOperation::LESS:
		out = VK_COMPARE_OP_LESS;
		break;
	case CompareOperation::EQUAL:
		out = VK_COMPARE_OP_EQUAL;
		break;
	case CompareOperation::LESS_EQUAL:
		out = VK_COMPARE_OP_LESS_OR_EQUAL;
		break;
	case CompareOperation::GREATER:
		out = VK_COMPARE_OP_GREATER;
		break;
	case CompareOperation::GREATER_EQUAL:
		out = VK_COMPARE_OP_GREATER_OR_EQUAL;
		break;
	case CompareOperation::NOT_EQUAL:
		out = VK_COMPARE_OP_NOT_EQUAL;
		break;
	case CompareOperation::NEVER:
		out = VK_COMPARE_OP_NEVER;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
class ConvertFormat
{
public:
	PixelFormat m_ak;
	VkFormat m_vk;

	ConvertFormat(const PixelFormat& ak, VkFormat vk)
		: m_ak(ak)
		, m_vk(vk)
	{
	}
};

#define ANKI_FMT(fmt, trf, vk)                                                 \
	ConvertFormat(PixelFormat(ComponentFormat::fmt, TransformFormat::trf), vk)

static const ConvertFormat CONVERT_FORMAT_TABLE[] = {
	ANKI_FMT(NONE, NONE, VK_FORMAT_R4G4_UNORM_PACK8),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R4G4B4A4_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B4G4R4A4_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R5G6B5_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B5G6R5_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R5G5B5A1_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B5G5R5A1_UNORM_PACK16),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A1R5G5B5_UNORM_PACK16),
	ANKI_FMT(R8, UNORM, VK_FORMAT_R8_UNORM),
	ANKI_FMT(R8, SNORM, VK_FORMAT_R8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8_SSCALED),
	ANKI_FMT(R8, UINT, VK_FORMAT_R8_UINT),
	ANKI_FMT(R8, SINT, VK_FORMAT_R8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8_SRGB),
	ANKI_FMT(R8G8, UNORM, VK_FORMAT_R8G8_UNORM),
	ANKI_FMT(R8G8, SNORM, VK_FORMAT_R8G8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8_SSCALED),
	ANKI_FMT(R8G8, UINT, VK_FORMAT_R8G8_UINT),
	ANKI_FMT(R8G8, SINT, VK_FORMAT_R8G8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8_SRGB),
	ANKI_FMT(R8G8B8, UNORM, VK_FORMAT_R8G8B8_UNORM),
	ANKI_FMT(R8G8B8, SNORM, VK_FORMAT_R8G8B8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8_SSCALED),
	ANKI_FMT(R8G8B8, UINT, VK_FORMAT_R8G8B8_UINT),
	ANKI_FMT(R8G8B8, SINT, VK_FORMAT_R8G8B8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8_SRGB),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_UNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_SSCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8_SRGB),
	ANKI_FMT(R8G8B8A8, UNORM, VK_FORMAT_R8G8B8A8_UNORM),
	ANKI_FMT(R8G8B8A8, SNORM, VK_FORMAT_R8G8B8A8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8A8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8A8_SSCALED),
	ANKI_FMT(R8G8B8A8, UINT, VK_FORMAT_R8G8B8A8_UINT),
	ANKI_FMT(R8G8B8A8, SINT, VK_FORMAT_R8G8B8A8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R8G8B8A8_SRGB),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_UNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_SSCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_B8G8R8A8_SRGB),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_UNORM_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_SNORM_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_USCALED_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_SSCALED_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_UINT_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_SINT_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A8B8G8R8_SRGB_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_UNORM_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_SNORM_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_USCALED_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_SSCALED_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_UINT_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2R10G10B10_SINT_PACK32),
	ANKI_FMT(R10G10B10A2, UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32),
	ANKI_FMT(R10G10B10A2, SNORM, VK_FORMAT_A2B10G10R10_SNORM_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2B10G10R10_USCALED_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_A2B10G10R10_SSCALED_PACK32),
	ANKI_FMT(R10G10B10A2, UINT, VK_FORMAT_A2B10G10R10_UINT_PACK32),
	ANKI_FMT(R10G10B10A2, SINT, VK_FORMAT_A2B10G10R10_SINT_PACK32),
	ANKI_FMT(R16, UNORM, VK_FORMAT_R16_UNORM),
	ANKI_FMT(R16, SNORM, VK_FORMAT_R16_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16_SSCALED),
	ANKI_FMT(R16, UINT, VK_FORMAT_R16_UINT),
	ANKI_FMT(R16, SINT, VK_FORMAT_R16_SINT),
	ANKI_FMT(R16, FLOAT, VK_FORMAT_R16_SFLOAT),
	ANKI_FMT(R16G16, UNORM, VK_FORMAT_R16G16_UNORM),
	ANKI_FMT(R16G16, SNORM, VK_FORMAT_R16G16_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16_SSCALED),
	ANKI_FMT(R16G16, UINT, VK_FORMAT_R16G16_UINT),
	ANKI_FMT(R16G16, SINT, VK_FORMAT_R16G16_SINT),
	ANKI_FMT(R16G16, FLOAT, VK_FORMAT_R16G16_SFLOAT),
	ANKI_FMT(R16G16B16, UNORM, VK_FORMAT_R16G16B16_UNORM),
	ANKI_FMT(R16G16B16, SNORM, VK_FORMAT_R16G16B16_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16B16_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16B16_SSCALED),
	ANKI_FMT(R16G16B16, UINT, VK_FORMAT_R16G16B16_UINT),
	ANKI_FMT(R16G16B16, SINT, VK_FORMAT_R16G16B16_SINT),
	ANKI_FMT(R16G16B16, FLOAT, VK_FORMAT_R16G16B16_SFLOAT),
	ANKI_FMT(R16G16B16A16, UNORM, VK_FORMAT_R16G16B16A16_UNORM),
	ANKI_FMT(R16G16B16A16, SNORM, VK_FORMAT_R16G16B16A16_SNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16B16A16_USCALED),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R16G16B16A16_SSCALED),
	ANKI_FMT(R16G16B16A16, UINT, VK_FORMAT_R16G16B16A16_UINT),
	ANKI_FMT(R16G16B16A16, SINT, VK_FORMAT_R16G16B16A16_SINT),
	ANKI_FMT(R16G16B16A16, FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT),
	ANKI_FMT(R32, UINT, VK_FORMAT_R32_UINT),
	ANKI_FMT(R32, SINT, VK_FORMAT_R32_SINT),
	ANKI_FMT(R32, FLOAT, VK_FORMAT_R32_SFLOAT),
	ANKI_FMT(R32G32, UINT, VK_FORMAT_R32G32_UINT),
	ANKI_FMT(R32G32, SINT, VK_FORMAT_R32G32_SINT),
	ANKI_FMT(R32G32, FLOAT, VK_FORMAT_R32G32_SFLOAT),
	ANKI_FMT(R32G32B32, UINT, VK_FORMAT_R32G32B32_UINT),
	ANKI_FMT(R32G32B32, SINT, VK_FORMAT_R32G32B32_SINT),
	ANKI_FMT(R32G32B32, FLOAT, VK_FORMAT_R32G32B32_SFLOAT),
	ANKI_FMT(R32G32B32A32, UINT, VK_FORMAT_R32G32B32A32_UINT),
	ANKI_FMT(R32G32B32A32, SINT, VK_FORMAT_R32G32B32A32_SINT),
	ANKI_FMT(R32G32B32A32, FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64_SFLOAT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64_SFLOAT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64_SFLOAT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64A64_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64A64_SINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_R64G64B64A64_SFLOAT),
	ANKI_FMT(R11G11B10, FLOAT, VK_FORMAT_B10G11R11_UFLOAT_PACK32),
	ANKI_FMT(NONE, NONE, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32),
	ANKI_FMT(D16, UNORM, VK_FORMAT_D16_UNORM),
	ANKI_FMT(NONE, NONE, VK_FORMAT_X8_D24_UNORM_PACK32),
	ANKI_FMT(D32, FLOAT, VK_FORMAT_D32_SFLOAT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_S8_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_D16_UNORM_S8_UINT),
	ANKI_FMT(D24, UNORM, VK_FORMAT_D24_UNORM_S8_UINT),
	ANKI_FMT(NONE, NONE, VK_FORMAT_D32_SFLOAT_S8_UINT),
	ANKI_FMT(R8G8B8_S3TC, NONE, VK_FORMAT_BC1_RGB_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC1_RGB_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC1_RGBA_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC1_RGBA_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC2_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC2_SRGB_BLOCK),
	ANKI_FMT(R8G8B8A8_S3TC, NONE, VK_FORMAT_BC3_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC3_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC4_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC4_SNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC5_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC5_SNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC6H_UFLOAT_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC6H_SFLOAT_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC7_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_BC7_SRGB_BLOCK),
	ANKI_FMT(R8G8B8_ETC2, NONE, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK),
	ANKI_FMT(R8G8B8A8_ETC2, NONE, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_EAC_R11_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_EAC_R11_SNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_EAC_R11G11_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_EAC_R11G11_SNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_4x4_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_4x4_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_5x4_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_5x4_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_5x5_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_5x5_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_6x5_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_6x5_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_6x6_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_6x6_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x5_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x5_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x6_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x6_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x8_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_8x8_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x5_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x5_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x6_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x6_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x8_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x8_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x10_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_10x10_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_12x10_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_12x10_SRGB_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_12x12_UNORM_BLOCK),
	ANKI_FMT(NONE, NONE, VK_FORMAT_ASTC_12x12_SRGB_BLOCK)};

#undef ANKI_FMT

const U CONVERT_FORMAT_TABLE_SIZE =
	sizeof(CONVERT_FORMAT_TABLE) / sizeof(CONVERT_FORMAT_TABLE[0]);

VkFormat convertFormat(PixelFormat ak)
{
	VkFormat out = VK_FORMAT_UNDEFINED;
	for(U i = 0; i < CONVERT_FORMAT_TABLE_SIZE; ++i)
	{
		const ConvertFormat& entry = CONVERT_FORMAT_TABLE[i];
		if(ak == entry.m_ak)
		{
			out = entry.m_vk;
		}
	}

	ANKI_ASSERT(out != VK_FORMAT_UNDEFINED && "No format in the table");
	return out;
}

//==============================================================================
VkPrimitiveTopology convertTopology(PrimitiveTopology ak)
{
	VkPrimitiveTopology out = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	switch(ak)
	{
	case POINTS:
		out = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		break;
	case LINES:
		out = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		break;
	case LINE_STIP:
		out = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		break;
	case TRIANGLES:
		out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		break;
	case TRIANGLE_STRIP:
		out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		break;
	case PATCHES:
		out = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkPolygonMode convertFillMode(FillMode ak)
{
	VkPolygonMode out = VK_POLYGON_MODE_FILL;
	switch(ak)
	{
	case FillMode::POINTS:
		out = VK_POLYGON_MODE_POINT;
		break;
	case FillMode::WIREFRAME:
		out = VK_POLYGON_MODE_LINE;
		break;
	case FillMode::SOLID:
		out = VK_POLYGON_MODE_FILL;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkCullModeFlags convertCullMode(CullMode ak)
{
	VkCullModeFlags out = 0;
	switch(ak)
	{
	case CullMode::FRONT:
		out = VK_CULL_MODE_FRONT_BIT;
		break;
	case CullMode::BACK:
		out = VK_CULL_MODE_BACK_BIT;
		break;
	case CullMode::FRONT_AND_BACK:
		out = VK_CULL_MODE_FRONT_BIT | VK_CULL_MODE_BACK_BIT;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkBlendFactor convertBlendMethod(BlendMethod ak)
{
	VkBlendFactor out = VK_BLEND_FACTOR_MAX_ENUM;
	switch(ak)
	{
	case BlendMethod::ZERO:
		out = VK_BLEND_FACTOR_ZERO;
		break;
	case BlendMethod::ONE:
		out = VK_BLEND_FACTOR_ONE;
		break;
	case BlendMethod::SRC_COLOR:
		out = VK_BLEND_FACTOR_SRC_COLOR;
		break;
	case BlendMethod::ONE_MINUS_SRC_COLOR:
		out = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case BlendMethod::DST_COLOR:
		out = VK_BLEND_FACTOR_DST_COLOR;
		break;
	case BlendMethod::ONE_MINUS_DST_COLOR:
		out = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		break;
	case BlendMethod::SRC_ALPHA:
		out = VK_BLEND_FACTOR_SRC_ALPHA;
		break;
	case BlendMethod::ONE_MINUS_SRC_ALPHA:
		out = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case BlendMethod::DST_ALPHA:
		out = VK_BLEND_FACTOR_DST_ALPHA;
		break;
	case BlendMethod::ONE_MINUS_DST_ALPHA:
		out = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case BlendMethod::CONSTANT_COLOR:
		out = VK_BLEND_FACTOR_CONSTANT_COLOR;
		break;
	case BlendMethod::ONE_MINUS_CONSTANT_COLOR:
		out = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		break;
	case BlendMethod::CONSTANT_ALPHA:
		out = VK_BLEND_FACTOR_CONSTANT_ALPHA;
		break;
	case BlendMethod::ONE_MINUS_CONSTANT_ALPHA:
		out = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		break;
	case BlendMethod::SRC_ALPHA_SATURATE:
		out = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		break;
	case BlendMethod::SRC1_COLOR:
		out = VK_BLEND_FACTOR_SRC1_COLOR;
		break;
	case BlendMethod::ONE_MINUS_SRC1_COLOR:
		out = VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		break;
	case BlendMethod::SRC1_ALPHA:
		out = VK_BLEND_FACTOR_SRC1_ALPHA;
		break;
	case BlendMethod::ONE_MINUS_SRC1_ALPHA:
		out = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkBlendOp convertBlendFunc(BlendFunction ak)
{
	VkBlendOp out = VK_BLEND_OP_MAX_ENUM;

	switch(ak)
	{
	case BlendFunction::ADD:
		out = VK_BLEND_OP_ADD;
		break;
	case BlendFunction::SUBTRACT:
		out = VK_BLEND_OP_SUBTRACT;
		break;
	case BlendFunction::REVERSE_SUBTRACT:
		out = VK_BLEND_OP_REVERSE_SUBTRACT;
		break;
	case BlendFunction::MIN:
		out = VK_BLEND_OP_MIN;
		break;
	case BlendFunction::MAX:
		out = VK_BLEND_OP_MAX;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkAttachmentLoadOp convertLoadOp(AttachmentLoadOperation ak)
{
	VkAttachmentLoadOp out = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;

	switch(ak)
	{
	case AttachmentLoadOperation::LOAD:
		out = VK_ATTACHMENT_LOAD_OP_LOAD;
		break;
	case AttachmentLoadOperation::CLEAR:
		out = VK_ATTACHMENT_LOAD_OP_CLEAR;
		break;
	case AttachmentLoadOperation::DONT_CARE:
		out = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkAttachmentStoreOp convertStoreOp(AttachmentStoreOperation ak)
{
	VkAttachmentStoreOp out = VK_ATTACHMENT_STORE_OP_MAX_ENUM;

	switch(ak)
	{
	case AttachmentStoreOperation::STORE:
		out = VK_ATTACHMENT_STORE_OP_STORE;
		break;
	case AttachmentStoreOperation::DONT_CARE:
		out = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkBufferUsageFlags convertBufferUsageBit(BufferUsageBit usageMask)
{
	VkBufferUsageFlags out = 0;

	if((usageMask & BufferUsageBit::UNIFORM_ANY_SHADER) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	if((usageMask & BufferUsageBit::STORAGE_ANY) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	if((usageMask & BufferUsageBit::INDEX) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}

	if((usageMask & BufferUsageBit::VERTEX) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}

	if((usageMask & BufferUsageBit::INDIRECT) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}

	if((usageMask & BufferUsageBit::TRANSFER_DESTINATION)
		!= BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	if((usageMask & BufferUsageBit::TRANSFER_SOURCE) != BufferUsageBit::NONE)
	{
		out |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}

	ANKI_ASSERT(out);

	return out;
}

//==============================================================================
VkImageType convertTextureType(TextureType ak)
{
	VkImageType out = VK_IMAGE_TYPE_MAX_ENUM;
	switch(ak)
	{
	case TextureType::CUBE:
	case TextureType::CUBE_ARRAY:
	case TextureType::_2D:
	case TextureType::_2D_ARRAY:
		out = VK_IMAGE_TYPE_2D;
		break;
	case TextureType::_3D:
		out = VK_IMAGE_TYPE_3D;
		break;
	case TextureType::_1D:
		out = VK_IMAGE_TYPE_1D;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkImageViewType convertTextureViewType(TextureType ak)
{
	VkImageViewType out = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	switch(ak)
	{
	case TextureType::_1D:
		out = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case TextureType::_2D:
		out = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case TextureType::_2D_ARRAY:
		out = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case TextureType::_3D:
		out = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case TextureType::CUBE:
		out = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case TextureType::CUBE_ARRAY:
		out = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	default:
		ANKI_ASSERT(0);
	}

	return out;
}

//==============================================================================
VkImageUsageFlags convertTextureUsage(
	TextureUsageBit ak, const PixelFormat& format)
{
	VkImageUsageFlags out = 0;

	if((ak & TextureUsageBit::ANY_SHADER_SAMPLED) != TextureUsageBit::NONE)
	{
		out |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	if((ak & TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE)
		!= TextureUsageBit::NONE)
	{
		if(formatIsDepthStencil(format))
		{
			out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
	}

	if((ak & TextureUsageBit::GENERATE_MIPMAPS) != TextureUsageBit::NONE)
	{
		out |=
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	if((ak & TextureUsageBit::UPLOAD) != TextureUsageBit::NONE)
	{
		out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	ANKI_ASSERT(out);
	return out;
}

} // end namespace anki

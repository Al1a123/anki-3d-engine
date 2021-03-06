// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/renderer/Pps.h>
#include <anki/renderer/Renderer.h>
#include <anki/renderer/Bloom.h>
#include <anki/renderer/Sslf.h>
#include <anki/renderer/Tm.h>
#include <anki/renderer/Is.h>
#include <anki/renderer/Ms.h>
#include <anki/renderer/Dbg.h>
#include <anki/util/Logger.h>
#include <anki/misc/ConfigSet.h>
#include <anki/scene/SceneNode.h>
#include <anki/scene/FrustumComponent.h>

namespace anki
{

//==============================================================================
const PixelFormat Pps::RT_PIXEL_FORMAT(
	ComponentFormat::R8G8B8, TransformFormat::UNORM);

//==============================================================================
Pps::Pps(Renderer* r)
	: RenderingPass(r)
{
}

//==============================================================================
Pps::~Pps()
{
}

//==============================================================================
Error Pps::initInternal(const ConfigSet& config)
{
	ANKI_ASSERT("Initializing PPS");

	// FBO
	m_r->createRenderTarget(m_r->getWidth(),
		m_r->getHeight(),
		RT_PIXEL_FORMAT,
		1,
		SamplingFilter::LINEAR,
		1,
		m_rt);

	FramebufferInitInfo fbInit;
	fbInit.m_colorAttachmentCount = 1;
	fbInit.m_colorAttachments[0].m_texture = m_rt;
	fbInit.m_colorAttachments[0].m_loadOperation =
		AttachmentLoadOperation::DONT_CARE;
	m_fb = getGrManager().newInstance<Framebuffer>(fbInit);

	// SProg
	StringAuto pps(getAllocator());

	pps.sprintf("#define BLOOM_ENABLED %u\n"
				"#define SHARPEN_ENABLED %u\n"
				"#define GAMMA_CORRECTION_ENABLED %u\n"
				"#define FBO_WIDTH %u\n"
				"#define FBO_HEIGHT %u\n",
		m_r->getBloomEnabled(),
		U(config.getNumber("pps.sharpen")),
		U(config.getNumber("pps.gammaCorrection")),
		m_r->getWidth(),
		m_r->getHeight());

	ANKI_CHECK(getResourceManager().loadResourceToCache(
		m_frag, "shaders/Pps.frag.glsl", pps.toCString(), "r_"));

	ColorStateInfo colorState;
	colorState.m_attachmentCount = 1;
	colorState.m_attachments[0].m_format = RT_PIXEL_FORMAT;
	m_r->createDrawQuadPipeline(m_frag->getGrShader(), colorState, m_ppline);

	// LUT
	ANKI_CHECK(loadColorGradingTexture("engine_data/DefaultLut.ankitex"));

	// RC goup
	ResourceGroupInitInfo rcInit;
	rcInit.m_textures[0].m_texture = m_r->getIs().getRt();

	if(m_r->getBloomEnabled())
	{
		rcInit.m_textures[1].m_texture = m_r->getBloom().getRt();
	}

	rcInit.m_textures[2].m_texture = m_lut->getGrTexture();

	rcInit.m_storageBuffers[0].m_buffer =
		m_r->getTm().getAverageLuminanceBuffer();

	m_rcGroup = getGrManager().newInstance<ResourceGroup>(rcInit);

	getGrManager().finish();
	return ErrorCode::NONE;
}

//==============================================================================
Error Pps::init(const ConfigSet& config)
{
	Error err = initInternal(config);
	if(err)
	{
		ANKI_LOGE("Failed to init PPS");
	}

	return err;
}
//==============================================================================
Error Pps::loadColorGradingTexture(CString filename)
{
	m_lut.reset(nullptr);
	ANKI_CHECK(getResourceManager().loadResource(filename, m_lut));
	return ErrorCode::NONE;
}

//==============================================================================
void Pps::run(RenderingContext& ctx)
{
	CommandBufferPtr& cmdb = ctx.m_commandBuffer;
	FramebufferPtr fb = m_fb;
	U32 width = m_r->getWidth();
	U32 height = m_r->getHeight();

	Bool isLastStage = !m_r->getDbg().getEnabled();
	if(isLastStage)
	{
		m_r->getOutputFramebuffer(fb, width, height);
	}

	cmdb->beginRenderPass(fb);
	cmdb->setViewport(0, 0, width, height);
	cmdb->bindPipeline(m_ppline);
	cmdb->bindResourceGroup(m_rcGroup, 0, nullptr);
	m_r->drawQuad(cmdb);
	cmdb->endRenderPass();
}

} // end namespace anki

// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/renderer/Is.h>
#include <anki/renderer/Renderer.h>
#include <anki/renderer/Sm.h>
#include <anki/renderer/Pps.h>
#include <anki/renderer/Ir.h>
#include <anki/renderer/Ms.h>
#include <anki/renderer/LightBin.h>
#include <anki/scene/FrustumComponent.h>
#include <anki/misc/ConfigSet.h>
#include <anki/util/HighRezTimer.h>

namespace anki
{

//==============================================================================
// Misc                                                                        =
//==============================================================================

struct ShaderCommonUniforms
{
	Vec4 m_projectionParams;
	Vec4 m_rendererSizeTimePad1;
	Vec4 m_nearFarClustererMagicPad1;
	Mat4 m_viewMat;
	Mat3x4 m_invViewRotation;
	UVec4 m_tileCount;
};

//==============================================================================
// Is                                                                          =
//==============================================================================

const PixelFormat Is::RT_PIXEL_FORMAT(
	ComponentFormat::R11G11B10, TransformFormat::FLOAT);

//==============================================================================
Is::Is(Renderer* r)
	: RenderingPass(r)
{
}

//==============================================================================
Is::~Is()
{
	if(m_lightBin)
	{
		getAllocator().deleteInstance(m_lightBin);
	}
}

//==============================================================================
Error Is::init(const ConfigSet& config)
{
	Error err = initInternal(config);

	if(err)
	{
		ANKI_LOGE("Failed to init IS");
	}

	return err;
}

//==============================================================================
Error Is::initInternal(const ConfigSet& config)
{
	m_maxLightIds = config.getNumber("is.maxLightsPerCluster");

	if(m_maxLightIds == 0)
	{
		ANKI_LOGE("Incorrect number of max light indices");
		return ErrorCode::USER_DATA;
	}

	U clusterCount = m_r->getTileCountXY().x() * m_r->getTileCountXY().y()
		* config.getNumber("clusterSizeZ");
	m_maxLightIds *= clusterCount;

	m_lightBin = getAllocator().newInstance<LightBin>(getAllocator(),
		m_r->getTileCountXY().x(),
		m_r->getTileCountXY().y(),
		config.getNumber("clusterSizeZ"),
		&m_r->getThreadPool(),
		&getGrManager());

	//
	// Load the programs
	//
	StringAuto pps(getAllocator());

	pps.sprintf("\n#define TILE_COUNT_X %u\n"
				"#define TILE_COUNT_Y %u\n"
				"#define CLUSTER_COUNT %u\n"
				"#define RENDERER_WIDTH %u\n"
				"#define RENDERER_HEIGHT %u\n"
				"#define MAX_LIGHT_INDICES %u\n"
				"#define POISSON %u\n"
				"#define INDIRECT_ENABLED %u\n"
				"#define IR_MIPMAP_COUNT %u\n",
		m_r->getTileCountXY().x(),
		m_r->getTileCountXY().y(),
		clusterCount,
		m_r->getWidth(),
		m_r->getHeight(),
		m_maxLightIds,
		m_r->getSmEnabled() ? m_r->getSm().getPoissonEnabled() : 0,
		m_r->getIrEnabled(),
		(m_r->getIrEnabled()) ? m_r->getIr().getCubemapArrayMipmapCount() : 0);

	// point light
	ANKI_CHECK(getResourceManager().loadResourceToCache(
		m_lightVert, "shaders/Is.vert.glsl", pps.toCString(), "r_"));

	ANKI_CHECK(getResourceManager().loadResourceToCache(
		m_lightFrag, "shaders/Is.frag.glsl", pps.toCString(), "r_"));

	PipelineInitInfo init;

	init.m_inputAssembler.m_topology = PrimitiveTopology::TRIANGLE_STRIP;
	init.m_depthStencil.m_depthWriteEnabled = false;
	init.m_depthStencil.m_depthCompareFunction = CompareOperation::ALWAYS;
	init.m_color.m_attachmentCount = 1;
	init.m_color.m_attachments[0].m_format = RT_PIXEL_FORMAT;
	init.m_shaders[U(ShaderType::VERTEX)] = m_lightVert->getGrShader();
	init.m_shaders[U(ShaderType::FRAGMENT)] = m_lightFrag->getGrShader();
	m_lightPpline = getGrManager().newInstance<Pipeline>(init);

	//
	// Create framebuffer
	//
	m_r->createRenderTarget(m_r->getWidth(),
		m_r->getHeight(),
		RT_PIXEL_FORMAT,
		1,
		SamplingFilter::LINEAR,
		IS_MIPMAP_COUNT,
		m_rt);

	FramebufferInitInfo fbInit;
	fbInit.m_colorAttachmentCount = 1;
	fbInit.m_colorAttachments[0].m_texture = m_rt;
	fbInit.m_colorAttachments[0].m_loadOperation =
		AttachmentLoadOperation::DONT_CARE;
	m_fb = getGrManager().newInstance<Framebuffer>(fbInit);

	//
	// Create resource group
	//
	{
		ResourceGroupInitInfo init;
		init.m_textures[0].m_texture = m_r->getMs().getRt0();
		init.m_textures[1].m_texture = m_r->getMs().getRt1();
		init.m_textures[2].m_texture = m_r->getMs().getRt2();
		init.m_textures[3].m_texture = m_r->getMs().getDepthRt();

		if(m_r->getSmEnabled())
		{
			init.m_textures[4].m_texture = m_r->getSm().getSpotTextureArray();
			init.m_textures[5].m_texture = m_r->getSm().getOmniTextureArray();
		}

		if(m_r->getIrEnabled())
		{
			init.m_textures[6].m_texture = m_r->getIr().getReflectionTexture();
			init.m_textures[7].m_texture = m_r->getIr().getIrradianceTexture();

			init.m_textures[8].m_texture = m_r->getIr().getIntegrationLut();
			init.m_textures[8].m_sampler =
				m_r->getIr().getIntegrationLutSampler();
		}

		init.m_uniformBuffers[0].m_uploadedMemory = true;
		init.m_uniformBuffers[1].m_uploadedMemory = true;
		init.m_uniformBuffers[2].m_uploadedMemory = true;
		if(m_r->getIrEnabled())
		{
			init.m_uniformBuffers[3].m_uploadedMemory = true;
		}

		init.m_storageBuffers[0].m_uploadedMemory = true;
		init.m_storageBuffers[1].m_uploadedMemory = true;

		m_rcGroup = getGrManager().newInstance<ResourceGroup>(init);
	}

	getGrManager().finish();
	return ErrorCode::NONE;
}

//==============================================================================
Error Is::binLights(RenderingContext& ctx)
{
	updateCommonBlock(ctx);

	ANKI_CHECK(m_lightBin->bin(*ctx.m_frustumComponent,
		getFrameAllocator(),
		m_maxLightIds,
		m_r->getSmEnabled(),
		ctx.m_is.m_dynBufferInfo.m_uniformBuffers[P_LIGHTS_LOCATION],
		ctx.m_is.m_dynBufferInfo.m_uniformBuffers[S_LIGHTS_LOCATION],
		m_r->getIrEnabled()
			? &ctx.m_is.m_dynBufferInfo.m_uniformBuffers[PROBES_LOCATION]
			: nullptr,
		ctx.m_is.m_dynBufferInfo.m_storageBuffers[CLUSTERS_LOCATION],
		ctx.m_is.m_dynBufferInfo.m_storageBuffers[LIGHT_IDS_LOCATION]));

	return ErrorCode::NONE;
}

//==============================================================================
void Is::run(RenderingContext& ctx)
{
	CommandBufferPtr& cmdb = ctx.m_commandBuffer;

	cmdb->beginRenderPass(m_fb);
	cmdb->setViewport(0, 0, m_r->getWidth(), m_r->getHeight());
	cmdb->bindPipeline(m_lightPpline);
	cmdb->bindResourceGroup(m_rcGroup, 0, &ctx.m_is.m_dynBufferInfo);
	cmdb->drawArrays(4, m_r->getTileCount());
	cmdb->endRenderPass();
}

//==============================================================================
void Is::updateCommonBlock(RenderingContext& ctx)
{
	const FrustumComponent& fr = *ctx.m_frustumComponent;
	ShaderCommonUniforms* blk = static_cast<ShaderCommonUniforms*>(
		getGrManager().allocateFrameTransientMemory(
			sizeof(ShaderCommonUniforms),
			BufferUsageBit::UNIFORM_ANY_SHADER,
			ctx.m_is.m_dynBufferInfo.m_uniformBuffers[COMMON_VARS_LOCATION]));

	// Start writing
	blk->m_projectionParams = fr.getProjectionParameters();
	blk->m_viewMat = fr.getViewMatrix().getTransposed();
	blk->m_nearFarClustererMagicPad1 = Vec4(fr.getFrustum().getNear(),
		fr.getFrustum().getFar(),
		m_lightBin->getClusterer().getShaderMagicValue(),
		0.0);

	blk->m_invViewRotation =
		Mat3x4(fr.getViewMatrix().getInverse().getRotationPart());

	blk->m_rendererSizeTimePad1 = Vec4(
		m_r->getWidth(), m_r->getHeight(), HighRezTimer::getCurrentTime(), 0.0);

	blk->m_tileCount = UVec4(m_r->getTileCountXY(), m_r->getTileCount(), 0);
}

} // end namespace anki

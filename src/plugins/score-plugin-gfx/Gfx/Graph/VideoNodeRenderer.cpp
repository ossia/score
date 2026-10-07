#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoderFactory.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flicks.hpp>

#include <QElapsedTimer>

extern "C"
{
#if __has_include(<libavutil/hdr_dynamic_metadata.h>)
#include <libavutil/hdr_dynamic_metadata.h>
#endif
#if __has_include(<libavutil/hdr_dynamic_vivid_metadata.h>)
#include <libavutil/hdr_dynamic_vivid_metadata.h>
#endif
}
namespace score::gfx
{

void VideoOwnTexture::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, const Port& output,
    const Video::ImageFormat& format, const VideoMaterialUBO& material,
    const QShader& vertex, const QShader& fragment, QRhiBuffer* processUBO,
    std::span<const Sampler> samplers)
{
  const QSize size{format.width, format.height};
  if(!outputGrabbedBySink(output) || size.isEmpty() || !vertex.isValid()
     || !fragment.isValid())
  {
    release();
    return;
  }

  const auto fmt = format.output_format == ::Video::OutputFormat::SDR
                       ? QRhiTexture::RGBA8
                       : QRhiTexture::RGBA16F;
  if(m_rt.texture
     && (m_rt.texture->pixelSize() != size || m_rt.texture->format() != fmt))
    release();

  auto& rhi = *renderer.state.rhi;
  if(!m_rt.texture)
  {
    m_rt = createRenderTarget(renderer.state, fmt, size, 1, false);
    m_rt.texture->setName("VideoOwnTexture::texture");
  }
  if(!m_material)
  {
    m_material = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(VideoMaterialUBO));
    m_material->setName("VideoOwnTexture::material");
    m_material->create();
  }
  if(!m_pipeline.pipeline)
    m_pipeline = buildPipeline(
        renderer, renderer.defaultQuad(), vertex, fragment, m_rt, processUBO,
        m_material, samplers);

  res.updateDynamicBuffer(m_material, 0, sizeof(VideoMaterialUBO), &material);
}

void VideoOwnTexture::render(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    const MeshBuffers& mesh)
{
  if(!m_pipeline.pipeline || m_renderedFrame == renderer.frame)
    return;
  m_renderedFrame = renderer.frame;

  const QSize size = m_rt.texture->pixelSize();
  cb.beginPass(m_rt.renderTarget, Qt::transparent, {0.0f, 0}, res);
  res = nullptr;
  cb.setGraphicsPipeline(m_pipeline.pipeline);
  cb.setShaderResources(m_pipeline.srb);
  cb.setViewport(QRhiViewport(0, 0, size.width(), size.height()));
  renderer.defaultQuad().draw(mesh, cb);
  cb.endPass();
}

void VideoOwnTexture::releasePipeline()
{
  m_pipeline.release();
}

void VideoOwnTexture::release()
{
  m_pipeline.release();
  m_rt.release();
  if(m_material)
    m_material->deleteLater();
  m_material = nullptr;
  m_renderedFrame = -1;
}

VideoNodeRenderer::VideoNodeRenderer(
    const VideoNodeBase& node, VideoFrameShare& frames) noexcept
    : NodeRenderer{node}
    , reader{frames}
    , m_frameFormat{decoder()}
{
  m_frameFormat.output_format = node.m_outputFormat;
  m_frameFormat.tonemap = node.m_tonemap;
  m_currentScaleMode = node.m_scaleMode;
}

VideoNodeRenderer::~VideoNodeRenderer() { }

Video::VideoMetadata& VideoNodeRenderer::decoder() const noexcept
{
  return *reader.m_decoder;
}

TextureRenderTarget VideoNodeRenderer::renderTargetForInput(const Port& input)
{
  return {};
}

void VideoNodeRenderer::createGpuDecoder()
{
  auto& model = const_cast<VideoNodeBase&>(node());
  auto& filter = model.m_filter;

  m_gpu = createGPUVideoDecoder(m_frameFormat, filter.toStdString());
  if(!m_gpu)
  {
    qDebug() << "Unhandled pixel format: '"
             << av_get_pix_fmt_name(m_frameFormat.pixel_format) << "'"
             << (uint32_t)(m_frameFormat.pixel_format);
    m_gpu = std::make_unique<EmptyDecoder>();
  }

  m_recomputeScale = true;
  m_currentFrameIdx = -1;
}

void VideoNodeRenderer::setupGpuDecoder(RenderList& r)
{
  if(m_gpu)
  {
    m_gpu->release(r);

    for(auto& p : m_p)
    {
      p.second.release();
    }
    m_p.clear();
  }

  m_shaders = {};
  m_ownTexture.releasePipeline();

  createGpuDecoder();

  createPipelines(r);
}

void VideoNodeRenderer::createPipelines(RenderList& r)
{
  if(m_gpu)
  {
    m_shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    score::gfx::defaultPassesInit(
        m_p, this->node().output[0]->edges, r, r.defaultQuad(), m_shaders.first,
        m_shaders.second, m_processUBO, m_materialUBO, m_gpu->samplers);
  }
}

void VideoNodeRenderer::checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h)
{
  // TODO won't work if VK is threaded and there are multiple windows
  const auto& n = this->node();
  auto src = decoder();
  src.output_format = n.m_outputFormat;
  src.tonemap = n.m_tonemap;

  if(videoDecoderNeedsRebuild(bool(m_gpu), m_frameFormat, src, fmt, w, h))
  {
    m_frameFormat.pixel_format = fmt;
    m_frameFormat.width = w;
    m_frameFormat.height
        = (src.interlacing == Video::Interlacing::Fields) ? h * 2 : h;
    m_frameFormat.output_format = n.m_outputFormat;
    m_frameFormat.tonemap = n.m_tonemap;
    m_frameFormat.color_space = src.color_space;
    m_frameFormat.color_range = src.color_range;
    m_frameFormat.color_trc = src.color_trc;
    m_frameFormat.color_primaries = src.color_primaries;
    m_frameFormat.interlacing = src.interlacing;

    setupGpuDecoder(r);
  }

  // Not part of the rebuild: the mode is a uniform in score_tc, so it is
  // picked up on the next material update without touching the pipeline.
  m_frameFormat.deinterlace = src.deinterlace;
}

void VideoNodeRenderer::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = renderer.defaultQuad();
  if(m_meshBuffer.buffers.empty())
  {
    m_meshBuffer = renderer.initMeshBuffer(mesh, res);
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("VideoNodeRenderer::init::m_processUBO");
  m_processUBO->create();

  m_materialUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
  m_materialUBO->setName("VideoNodeRenderer::init::m_materialUBO");
  m_materialUBO->create();

  if(!m_gpu)
    createGpuDecoder();

  // Cache the shaders from the GPU decoder (also creates its samplers/textures)
  if(m_gpu)
    m_shaders = m_gpu->init(renderer);

  m_recomputeScale = true;
  m_initialized = true;
}

void VideoNodeRenderer::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(!m_gpu)
    return;
  if(!m_shaders.first.isValid() || !m_shaders.second.isValid())
    return;

  auto rt = renderer.renderTargetForOutput(edge);
  if(rt.renderTarget)
  {
    auto pip = score::gfx::buildPipeline(
        renderer, renderer.defaultQuad(), m_shaders.first, m_shaders.second, rt,
        m_processUBO, m_materialUBO, m_gpu->samplers);
    if(pip.pipeline)
      m_p.emplace_back(&edge, Pass{rt, pip, nullptr});
  }
}

void VideoNodeRenderer::removeOutputPass(RenderList& renderer, Edge& edge)
{
  auto it
      = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
  if(it != m_p.end())
  {
    it->second.release();
    m_p.erase(it);
  }
}

bool VideoNodeRenderer::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
         != m_p.end();
}

void VideoNodeRenderer::releaseState(RenderList& r)
{
  if(!m_initialized)
    return;

  if(m_gpu)
    m_gpu->release(r);

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_materialUBO;
  m_materialUBO = nullptr;

  for(auto& p : m_p)
    p.second.release();
  m_p.clear();
  m_ownTexture.release();

  m_meshBuffer = {};
  m_shaders = {};

  if(m_currentFrame)
  {
    m_currentFrame->use_count--;
    m_currentFrame.reset();
  }

  m_initialized = false;
}

void VideoNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  for(Edge* edge : this->node().output[0]->edges)
  {
    addOutputPass(renderer, *edge, res);
  }
}

void VideoNodeRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  if(!m_gpu || !m_gpu->hasFrame)
    return;
  score::gfx::quadRenderPass(renderer, m_meshBuffer, cb, edge, m_p);
}

// TODO if we have multiple renderers for the same video, we must always keep
// a frame because rendered may have different rates, so we cannot know "when"
// all renderers have rendered, thue the pattern in the following function
// is not enough
void VideoNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node().standardUBO);

  auto reader_frame = reader.m_currentFrameIdx;
  if(reader_frame > this->m_currentFrameIdx)
  {
    auto old_frame = m_currentFrame;

    //std::lock_guard<std::mutex> lck{const_cast<VideoNode&>(node).reader.m_frameLock};
    if((m_currentFrame = reader.currentFrame()))
    {
      displayFrame(*m_currentFrame->frame, renderer, res);
    }

    if(old_frame)
      old_frame->use_count--;
    // TODO else ? fill with zeroes ?... does not that give green with YUV?

    this->m_currentFrameIdx = reader_frame;
  }

  // The scale fits the frame to the target it is drawn into, which a resize of
  // the output changes without re-initialising this renderer.
  const QSize renderSize = renderer.renderSize(edge);
  if(m_recomputeScale || m_currentScaleMode != this->node().m_scaleMode
     || renderSize != m_scaleRenderSize)
  {
    m_currentScaleMode = this->node().m_scaleMode;
    m_scaleRenderSize = renderSize;
    auto sz = computeScaleForMeshSizing(
        m_currentScaleMode, renderSize,
        QSizeF(m_frameFormat.width, m_frameFormat.height));
    Material mat;
    mat.scale[0] = sz.width();
    mat.scale[1] = sz.height();
    mat.textureSize[0] = this->m_frameFormat.width;
    mat.textureSize[1] = this->m_frameFormat.height;
    mat.field[0] = m_fieldParity;
    mat.field[1] = videoFieldMode(
        m_frameFormat.interlacing, m_frameFormat.deinterlace, m_fieldPartnerValid);

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }

  if(m_gpu)
  {
    Material own;
    own.textureSize[0] = m_frameFormat.width;
    own.textureSize[1] = m_frameFormat.height;
    own.field[0] = m_fieldParity;
    own.field[1] = videoFieldMode(
        m_frameFormat.interlacing, m_frameFormat.deinterlace, m_fieldPartnerValid);
    m_ownTexture.update(
        renderer, res, *this->node().output[0], m_frameFormat, own, m_shaders.first,
        m_shaders.second, m_processUBO, m_gpu->samplers);
  }
}

void VideoNodeRenderer::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    Edge& edge)
{
  if(m_gpu && m_gpu->hasFrame)
    m_ownTexture.render(renderer, cb, res, m_meshBuffer);
}

QRhiTexture* VideoNodeRenderer::textureForOutput(const Port& output)
{
  return m_ownTexture.texture();
}

void VideoNodeRenderer::displayFrame(
    AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(frame.data[0] == nullptr)
    return;

  /* FIXME dynamic HDR support
  auto* sd = av_frame_get_side_data(&frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
  auto* sd10p = av_frame_get_side_data(&frame, AV_FRAME_DATA_DYNAMIC_HDR_PLUS);
  auto* sdVivid = av_frame_get_side_data(&frame, AV_FRAME_DATA_DYNAMIC_HDR_VIVID);

  float scenePeakNits = 100.;
  if(sd10p)
  {
    auto* h = reinterpret_cast<const AVDynamicHDRPlus*>(sd10p->data);
    for(int i = 0; i < 3; i++)
      scenePeakNits = std::max(scenePeakNits,
                               float(av_q2d(h->params[0].maxscl[i])) * 10000.f);
  }
  else if(sdVivid)
  {
    auto* v = reinterpret_cast<const AVDynamicHDRVivid*>(sdVivid->data);
    scenePeakNits = float(av_q2d(v->params[0].maximum_maxrgb)) * 10000.f;
  }
  else if(sd && sd->data)
  {
    scenePeakNits = reinterpret_cast<AVContentLightMetadata*>(sd->data)->MaxCLL;
  }
  */

  // Which field this is, for score_tc. NDI's field_0 is the EVEN lines, which
  // the input marks as top-field-first; field_1 is the odd ones. A progressive
  // or woven frame leaves this at 0, where it is ignored.
  if(m_frameFormat.interlacing == Video::Interlacing::Fields)
  {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
    const bool topField = (frame.flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
#else
    const bool topField = frame.top_field_first != 0;
#endif
    const float parity = topField ? 0.f : 1.f;

    // The other half holds this field's partner only if the field before it was
    // the other parity. Two of the same parity in a row means one was dropped
    // and the partner half is a frame too old to weave with.
    const bool partner = m_sawField && parity != m_fieldParity;
    if(parity != m_fieldParity || partner != m_fieldPartnerValid)
      m_recomputeScale = true;  // the material carries both; refresh it

    m_fieldParity = parity;
    m_fieldPartnerValid = partner;
    m_sawField = true;
  }

  checkFormat(
      renderer, static_cast<AVPixelFormat>(frame.format), frame.width, frame.height);

  if(m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
    m_gpu->hasFrame = true;

    // A decoder that defers a mid-stream format change (e.g. HWTransferDecoder)
    // leaves its plane textures/samplers stale but still bound in our SRBs.
    // Rebuild decoder + pipelines together so textures and SRBs are recreated
    // in lockstep instead of freeing textures still referenced by the SRBs.
    if(m_gpu->formatChanged)
      setupGpuDecoder(renderer);
  }
}

void VideoNodeRenderer::release(RenderList& r)
{
  releaseState(r);
}
}

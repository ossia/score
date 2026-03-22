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

  createGpuDecoder();

  createPipelines(r);
}

void VideoNodeRenderer::createPipelines(RenderList& r)
{
  if(m_gpu)
  {
    auto shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    score::gfx::defaultPassesInit(
        m_p, this->node().output[0]->edges, r, r.defaultQuad(), shaders.first,
        shaders.second, m_processUBO, m_materialUBO, m_gpu->samplers);
  }
}

void VideoNodeRenderer::checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h)
{
  // TODO won't work if VK is threaded and there are multiple windows
  const auto& n = this->node();
  if(!m_gpu
     || fmt != m_frameFormat.pixel_format
     || w != m_frameFormat.width
     || h != m_frameFormat.height
     || n.m_outputFormat != m_frameFormat.output_format
     || n.m_tonemap != m_frameFormat.tonemap)
  {
    m_frameFormat.pixel_format = fmt;
    m_frameFormat.width = w;
    m_frameFormat.height = h;
    m_frameFormat.output_format = n.m_outputFormat;
    m_frameFormat.tonemap = n.m_tonemap;

    setupGpuDecoder(r);
  }
}

void VideoNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
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

  createPipelines(renderer);
  m_recomputeScale = true;
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

  if(m_recomputeScale || m_currentScaleMode != this->node().m_scaleMode)
  {
    m_currentScaleMode = this->node().m_scaleMode;
    auto sz = computeScaleForMeshSizing(
        m_currentScaleMode, renderer.renderSize(edge),
        QSizeF(m_frameFormat.width, m_frameFormat.height));
    Material mat;
    mat.scale_w = sz.width();
    mat.scale_h = sz.height();
    mat.tex_w = this->m_frameFormat.width;
    mat.tex_h = this->m_frameFormat.height;

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }
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

  checkFormat(
      renderer, static_cast<AVPixelFormat>(frame.format), frame.width, frame.height);

  if(m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
    m_gpu->hasFrame = true;
  }
}

void VideoNodeRenderer::release(RenderList& r)
{
  if(m_gpu)
    m_gpu->release(r);

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_materialUBO;
  m_materialUBO = nullptr;

  for(auto& p : m_p)
    p.second.release();
  m_p.clear();

  m_meshBuffer = {};

  if(m_currentFrame)
  {
    m_currentFrame->use_count--;
    m_currentFrame.reset();
  }
}
}

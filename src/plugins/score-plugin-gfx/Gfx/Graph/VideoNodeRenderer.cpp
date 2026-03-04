#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Gfx/Graph/decoders/DXV.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/HAP.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/NV16.hpp>
#include <Gfx/Graph/decoders/NV24.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/P016.hpp>
#include <Gfx/Graph/decoders/P210.hpp>
#include <Gfx/Graph/decoders/P410.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/VUYA.hpp>
#include <Gfx/Graph/decoders/Y210.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/YUV420P12.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV422P12.hpp>
#include <Gfx/Graph/decoders/YUV440.hpp>
#include <Gfx/Graph/decoders/YUV444.hpp>
#include <Gfx/Graph/decoders/YUV444P10.hpp>
#include <Gfx/Graph/decoders/YUV444P12.hpp>
#include <Gfx/Graph/decoders/YUVA420.hpp>
#include <Gfx/Graph/decoders/YUVA444.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>

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
  switch(m_frameFormat.pixel_format)
  {
    // 420P
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      m_gpu = std::make_unique<YUV420Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV420P10LE:
      m_gpu = std::make_unique<YUV420P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV420P12LE:
      m_gpu = std::make_unique<YUV420P12Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_NV12:
      m_gpu = std::make_unique<NV12Decoder>(m_frameFormat, false);
      break;
    case AV_PIX_FMT_NV21:
      m_gpu = std::make_unique<NV12Decoder>(m_frameFormat, true);
      break;
    case AV_PIX_FMT_P010LE:
      m_gpu = std::make_unique<P010Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_P016LE:
      m_gpu = std::make_unique<P016Decoder>(m_frameFormat);
      break;

    // 420P + alpha
    case AV_PIX_FMT_YUVA420P:
      m_gpu = std::make_unique<YUVA420Decoder>(m_frameFormat);
      break;

    // 444P
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
      m_gpu = std::make_unique<YUV444Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV444P10LE:
      m_gpu = std::make_unique<YUV444P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV444P12LE:
      m_gpu = std::make_unique<YUV444P12Decoder>(m_frameFormat);
      break;

    // 444P + alpha (ProRes 4444)
    case AV_PIX_FMT_YUVA444P:
      m_gpu = std::make_unique<YUVA444Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUVA444P10LE:
      m_gpu = std::make_unique<YUVA444P10Decoder>(m_frameFormat);
      break;

    // 440P
    case AV_PIX_FMT_YUV440P:
    case AV_PIX_FMT_YUVJ440P:
      m_gpu = std::make_unique<YUV440Decoder>(m_frameFormat);
      break;

    // 422P
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      m_gpu = std::make_unique<YUV422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV422P10LE:
      m_gpu = std::make_unique<YUV422P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV422P12LE:
      m_gpu = std::make_unique<YUV422P12Decoder>(m_frameFormat);
      break;

    // Semi-planar 422
    case AV_PIX_FMT_NV16:
      m_gpu = std::make_unique<NV16Decoder>(m_frameFormat);
      break;

    // YUYV
    case AV_PIX_FMT_UYVY422:
      m_gpu = std::make_unique<UYVY422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUYV422:
      m_gpu = std::make_unique<YUYV422Decoder>(m_frameFormat);
      break;

    // RGB24
    case AV_PIX_FMT_RGB24:
      m_gpu = std::make_unique<RGB24Decoder>(
          m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGR24:
      m_gpu = std::make_unique<RGB24Decoder>(
          m_frameFormat, "processed.rgb = tex.bgr; processed.a = 1.0; " + filter);
      break;

    // RGB48
    case AV_PIX_FMT_RGB48LE:
      m_gpu = std::make_unique<RGB48Decoder>(
          m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGR48LE:
      m_gpu = std::make_unique<RGB48Decoder>(
          m_frameFormat, "processed.rgb = tex.bgr; processed.a = 1.0; " + filter);
      break;

    // RGBA
    case AV_PIX_FMT_RGB0:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_RGBA:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, filter);
      break;
    case AV_PIX_FMT_BGR0:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGRA:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, m_frameFormat, filter);
      break;
    case AV_PIX_FMT_ARGB:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, "processed.rgba = tex.yzwx; " + filter);
      break;
    case AV_PIX_FMT_ABGR:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, "processed.rgba = tex.abgr; " + filter);
      break;

    // RGBA 16-bit per channel
    case AV_PIX_FMT_RGBA64LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA16F, 8, m_frameFormat, filter);
      break;
    case AV_PIX_FMT_BGRA64LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA16F, 8, m_frameFormat,
          "processed.rgba = vec4(tex.b, tex.g, tex.r, tex.a); " + filter);
      break;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    // Packed 10-bit RGB (X2RGB10: MSB 2X 10R 10G 10B LSB)
    // RGB10A2 reads as LSB: R10 G10 B10 A2, so for X2RGB10 we get B,G,R,X
    case AV_PIX_FMT_X2RGB10LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGB10A2, 4, m_frameFormat,
          "processed.rgba = vec4(tex.b, tex.g, tex.r, 1.0); " + filter);
      break;
#endif

    // Planar RGB
    case AV_PIX_FMT_GBRP:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R8, 1, "gbr", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GBRAP:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R8, 1, "gbra", m_frameFormat, filter);
      break;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GBRP10LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", m_frameFormat,
          "processed.rgb *= 64.0; " + filter);
      break;
    case AV_PIX_FMT_GBRP12LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", m_frameFormat,
          "processed.rgb *= 16.0; " + filter);
      break;
    case AV_PIX_FMT_GBRP16LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GBRAP10LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", m_frameFormat,
          "processed *= 64.0; " + filter);
      break;
    case AV_PIX_FMT_GBRAP12LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", m_frameFormat,
          "processed *= 16.0; " + filter);
      break;
    case AV_PIX_FMT_GBRAP16LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GBRPF32LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbr", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GBRAPF32LE:
      m_gpu = std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbra", m_frameFormat, filter);
      break;

    // Semi-planar 4:2:2 / 4:4:4 10-bit (ffmpeg 5.0+)
    case AV_PIX_FMT_NV24:
      m_gpu = std::make_unique<NV24Decoder>(m_frameFormat, false);
      break;
    case AV_PIX_FMT_NV42:
      m_gpu = std::make_unique<NV24Decoder>(m_frameFormat, true);
      break;

    // Packed YUV 4:2:2 10-bit (Y210)
    case AV_PIX_FMT_Y210LE:
      m_gpu = std::make_unique<Y210Decoder>(m_frameFormat);
      break;
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case AV_PIX_FMT_X2BGR10LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGB10A2, 4, m_frameFormat, "processed.a = 1.0; " + filter);
      break;
#endif
    case AV_PIX_FMT_P210LE:
      m_gpu = std::make_unique<P210Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_P410LE:
      m_gpu = std::make_unique<P410Decoder>(m_frameFormat);
      break;
#endif

    // Grey
    case AV_PIX_FMT_GRAY8:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    case AV_PIX_FMT_GRAY16:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(60, 8, 100)
    case AV_PIX_FMT_GRAYF16:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R16F, 2, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    // RGBA float32
    case AV_PIX_FMT_RGBAF32LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA32F, 16, m_frameFormat, filter);
      break;

    // Packed YUV 4:4:4 (VUYA / VUYX)
    case AV_PIX_FMT_VUYA:
      m_gpu = std::make_unique<VUYADecoder>(m_frameFormat, false);
      break;
    case AV_PIX_FMT_VUYX:
      m_gpu = std::make_unique<VUYADecoder>(m_frameFormat, true);
      break;
#endif
    case AV_PIX_FMT_GRAYF32:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R32F, 4, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;

    // Grey + Alpha
    case AV_PIX_FMT_YA8:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RG8, 2, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, tex.g);" + filter);
      break;
    case AV_PIX_FMT_YA16LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RG16, 4, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, tex.g);" + filter);
      break;

    default: {
      // try to read format as a 4cc
      std::string_view fourcc{(const char*)&m_frameFormat.pixel_format, 4};

      if(fourcc == "Hap1")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, m_frameFormat, filter);
      else if(fourcc == "Hap5")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, m_frameFormat, filter);
      else if(fourcc == "HapY")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, m_frameFormat, HAPDefaultDecoder::ycocg_filter + filter);
      else if(fourcc == "HapM")
        m_gpu = std::make_unique<HAPMDecoder>(m_frameFormat, filter);
      else if(fourcc == "HapA")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, m_frameFormat, filter);
      else if(fourcc == "Hap7")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, m_frameFormat, filter);
      else if(fourcc == "HapH")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC6H, m_frameFormat, filter);
      // DXV fourcc-based formats
      else if(fourcc == "Dxv1")
        m_gpu = std::make_unique<DXVDecoder>(
            QRhiTexture::BC1, m_frameFormat, filter);
      else if(fourcc == "Dxv5")
        m_gpu = std::make_unique<DXVDecoder>(
            QRhiTexture::BC3, m_frameFormat, filter);
      else if(fourcc == "DxvY")
        m_gpu = std::make_unique<DXVYCoCgDecoder>(false, m_frameFormat, filter);
      else if(fourcc == "DxvA")
        m_gpu = std::make_unique<DXVYCoCgDecoder>(true, m_frameFormat, filter);

      if(!m_gpu)
      {
        qDebug() << "Unhandled pixel format: '"
                 << av_get_pix_fmt_name(m_frameFormat.pixel_format) << "'"
                 << (uint32_t)(m_frameFormat.pixel_format);
        m_gpu = std::make_unique<EmptyDecoder>();
      }
      break;
    }
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

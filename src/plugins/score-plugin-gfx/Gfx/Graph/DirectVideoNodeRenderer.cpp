#include <Gfx/Graph/DirectVideoNodeRenderer.hpp>
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

#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace score::gfx
{

DirectVideoNodeRenderer::DirectVideoNodeRenderer(
    const VideoNodeBase& node, const Video::VideoMetadata& metadata) noexcept
    : NodeRenderer{node}
    , m_filePath{metadata.filePath}
    , m_fps{metadata.fps}
    , m_flicks_per_dts{metadata.flicks_per_dts}
    , m_dts_per_flicks{metadata.dts_per_flicks}
    , m_frameFormat{metadata}
{
  m_frameFormat.output_format = node.m_outputFormat;
  m_frameFormat.tonemap = node.m_tonemap;
  m_currentScaleMode = node.m_scaleMode;
}

DirectVideoNodeRenderer::~DirectVideoNodeRenderer()
{
  closeFile();
}

TextureRenderTarget DirectVideoNodeRenderer::renderTargetForInput(const Port& input)
{
  return {};
}

bool DirectVideoNodeRenderer::openFile()
{
  if(m_filePath.empty())
    return false;

  if(avformat_open_input(&m_formatContext, m_filePath.c_str(), nullptr, nullptr) != 0)
    return false;

  if(avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    closeFile();
    return false;
  }

  // Find video stream
  int stream = -1;
  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if(m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(stream == -1)
      {
        stream = i;
        continue;
      }
    }
    m_formatContext->streams[i]->discard = AVDISCARD_ALL;
  }

  if(stream == -1)
  {
    closeFile();
    return false;
  }

  m_avstream = m_formatContext->streams[stream];
  auto codecPar = m_avstream->codecpar;

  // HAP: no codec needed, raw packet data goes directly to GPU
  if(codecPar->codec_id == AV_CODEC_ID_HAP)
  {
    m_useAVCodec = false;
    memcpy(&m_frameFormat.pixel_format, &codecPar->codec_tag, 4);
    m_frameFormat.width = codecPar->width;
    m_frameFormat.height = codecPar->height;
    return true;
  }

  // DXV: peek first packet to determine sub-format, then GPU-direct for DXT1/DXT5
  if(codecPar->codec_id == AV_CODEC_ID_DXV)
  {
    auto packet = av_packet_alloc();
    bool dxv_ok = false;
    if(av_read_frame(m_formatContext, packet) >= 0 && packet->size >= 4)
    {
      uint32_t tag = packet->data[0] | (packet->data[1] << 8)
                     | (packet->data[2] << 16)
                     | ((uint32_t)packet->data[3] << 24);
      switch(tag)
      {
        case 0x44585431: // MKBETAG('D','X','T','1')
          memcpy(&m_frameFormat.pixel_format, "Dxv1", 4);
          dxv_ok = true;
          break;
        case 0x44585435: // MKBETAG('D','X','T','5')
          memcpy(&m_frameFormat.pixel_format, "Dxv5", 4);
          dxv_ok = true;
          break;
        case 0x59434736: // MKBETAG('Y','C','G','6')
          memcpy(&m_frameFormat.pixel_format, "DxvY", 4);
          dxv_ok = true;
          break;
        case 0x59473130: // MKBETAG('Y','G','1','0')
          memcpy(&m_frameFormat.pixel_format, "DxvA", 4);
          dxv_ok = true;
          break;
        default: {
          // Old format: check type flags in high byte
          uint8_t old_type = tag >> 24;
          if(old_type & 0x40)
          {
            memcpy(&m_frameFormat.pixel_format, "Dxv5", 4);
            dxv_ok = true;
          }
          else if(old_type & 0x20)
          {
            memcpy(&m_frameFormat.pixel_format, "Dxv1", 4);
            dxv_ok = true;
          }
          break;
        }
      }
      av_packet_unref(packet);
    }
    av_packet_free(&packet);
    // Seek back to beginning
    av_seek_frame(m_formatContext, m_avstream->index, 0, AVSEEK_FLAG_BACKWARD);

    if(dxv_ok)
    {
      m_useAVCodec = false;
      m_frameFormat.width = codecPar->width;
      m_frameFormat.height = codecPar->height;
      return true;
    }
    // else: YCG6/YG10 or unknown — fall through to avcodec
  }

  auto codec = avcodec_find_decoder(codecPar->codec_id);
  if(!codec)
  {
    closeFile();
    return false;
  }
  m_codec = codec;

  m_codecContext = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(m_codecContext, codecPar);
  m_codecContext->pkt_timebase = m_avstream->time_base;
  m_codecContext->thread_count = 1;

  int err = avcodec_open2(m_codecContext, codec, nullptr);
  if(err < 0)
  {
    avcodec_free_context(&m_codecContext);
    closeFile();
    return false;
  }

  // Update timing from codec context
  auto tb = m_codecContext->pkt_timebase;
  m_dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
  m_flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

  // Allocate the reusable frame
  m_decodedFrame = av_frame_alloc();

  return true;
}

void DirectVideoNodeRenderer::closeFile()
{
  if(m_decodedFrame)
  {
    av_frame_free(&m_decodedFrame);
    m_decodedFrame = nullptr;
  }

  if(m_codecContext)
  {
    avcodec_flush_buffers(m_codecContext);
    avcodec_free_context(&m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  if(m_formatContext)
  {
    avio_flush(m_formatContext->pb);
    avformat_flush(m_formatContext);
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  m_avstream = nullptr;
}

bool DirectVideoNodeRenderer::seekAndDecode(int64_t flicks)
{
  if(!m_formatContext || !m_avstream)
    return false;

  // Clamp negative flicks to 0 (beginning of file)
  if(flicks < 0)
    flicks = 0;

  // For HAP: seek and read raw packet
  if(!m_useAVCodec)
  {
    if(!ossia::seek_to_flick(m_formatContext, nullptr, m_avstream, flicks,
                             AVSEEK_FLAG_BACKWARD))
      return false;

    auto packet = av_packet_alloc();
    bool found = false;
    int attempts = 0;

    while(av_read_frame(m_formatContext, packet) >= 0 && attempts < 10)
    {
      attempts++;
      if(packet->stream_index == m_avstream->index)
      {
        auto cp = m_avstream->codecpar;
        if(!m_decodedFrame)
          m_decodedFrame = av_frame_alloc();

        // Clear previous buffer
        if(m_decodedFrame->buf[0])
          av_buffer_unref(&m_decodedFrame->buf[0]);

        m_decodedFrame->buf[0] = av_buffer_ref(packet->buf);
        m_decodedFrame->width = cp->width;
        m_decodedFrame->height = cp->height;
        m_decodedFrame->format = cp->codec_tag;
        m_decodedFrame->data[0] = packet->data;
        m_decodedFrame->linesize[0] = packet->size;
        m_decodedFrame->pts = packet->pts;
        m_decodedFrame->pkt_dts = packet->dts;

        m_lastDecodedDts = packet->dts;
        found = true;
        av_packet_unref(packet);
        break;
      }
      av_packet_unref(packet);
    }

    av_packet_free(&packet);
    return found;
  }

  // For codecs using avcodec
  if(!m_codecContext || !m_decodedFrame)
    return false;

  // Unref previous decoded frame data before seeking
  av_frame_unref(m_decodedFrame);

  if(!ossia::seek_to_flick(m_formatContext, m_codecContext, m_avstream, flicks,
                           AVSEEK_FLAG_BACKWARD))
    return false;

  auto packet = av_packet_alloc();
  bool found = false;
  int attempts = 0;

  while(av_read_frame(m_formatContext, packet) >= 0 && attempts < 30)
  {
    attempts++;
    if(packet->stream_index != m_avstream->index)
    {
      av_packet_unref(packet);
      continue;
    }

    int ret = avcodec_send_packet(m_codecContext, packet);
    av_packet_unref(packet);
    if(ret < 0 && ret != AVERROR(EAGAIN))
      break;

    ret = avcodec_receive_frame(m_codecContext, m_decodedFrame);
    if(ret == 0)
    {
      // Got a frame
      m_lastDecodedDts = m_decodedFrame->pkt_dts;

      // Update format if needed
      if(m_decodedFrame->width > 0 && m_decodedFrame->height > 0)
      {
        m_frameFormat.pixel_format = static_cast<AVPixelFormat>(m_decodedFrame->format);
        m_frameFormat.width = m_decodedFrame->width;
        m_frameFormat.height = m_decodedFrame->height;
      }

      found = true;
      break;
    }
    else if(ret != AVERROR(EAGAIN))
    {
      break;
    }
    // EAGAIN: need more packets, continue reading
  }

  av_packet_free(&packet);
  return found;
}

// createGpuDecoder is identical to VideoNodeRenderer::createGpuDecoder
// We delegate to the same factory logic
void DirectVideoNodeRenderer::createGpuDecoder()
{
  auto& model = const_cast<VideoNodeBase&>(node());
  auto& filter = model.m_filter;
  switch(m_frameFormat.pixel_format)
  {
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
    case AV_PIX_FMT_YUVA420P:
      m_gpu = std::make_unique<YUVA420Decoder>(m_frameFormat);
      break;
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
    case AV_PIX_FMT_YUVA444P:
      m_gpu = std::make_unique<YUVA444Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUVA444P10LE:
      m_gpu = std::make_unique<YUVA444P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV440P:
    case AV_PIX_FMT_YUVJ440P:
      m_gpu = std::make_unique<YUV440Decoder>(m_frameFormat);
      break;
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
    case AV_PIX_FMT_NV16:
      m_gpu = std::make_unique<NV16Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_UYVY422:
      m_gpu = std::make_unique<UYVY422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUYV422:
      m_gpu = std::make_unique<YUYV422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_RGB24:
      m_gpu = std::make_unique<RGB24Decoder>(
          m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGR24:
      m_gpu = std::make_unique<RGB24Decoder>(
          m_frameFormat, "processed.rgb = tex.bgr; processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_RGB48LE:
      m_gpu = std::make_unique<RGB48Decoder>(
          m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGR48LE:
      m_gpu = std::make_unique<RGB48Decoder>(
          m_frameFormat, "processed.rgb = tex.bgr; processed.a = 1.0; " + filter);
      break;
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
    case AV_PIX_FMT_X2RGB10LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGB10A2, 4, m_frameFormat,
          "processed.rgba = vec4(tex.b, tex.g, tex.r, 1.0); " + filter);
      break;
#endif
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
    case AV_PIX_FMT_NV24:
      m_gpu = std::make_unique<NV24Decoder>(m_frameFormat, false);
      break;
    case AV_PIX_FMT_NV42:
      m_gpu = std::make_unique<NV24Decoder>(m_frameFormat, true);
      break;
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
    case AV_PIX_FMT_RGBAF32LE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA32F, 16, m_frameFormat, filter);
      break;
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
      // HAP fourcc-based formats
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
        qDebug() << "DirectVideoNodeRenderer: Unhandled pixel format: '"
                 << av_get_pix_fmt_name(m_frameFormat.pixel_format) << "'"
                 << (uint32_t)(m_frameFormat.pixel_format);
        m_gpu = std::make_unique<EmptyDecoder>();
      }
      break;
    }
  }

  m_recomputeScale = true;
}

void DirectVideoNodeRenderer::setupGpuDecoder(RenderList& r)
{
  if(m_gpu)
  {
    m_gpu->release(r);
    for(auto& p : m_p)
      p.second.release();
    m_p.clear();
  }

  createGpuDecoder();
  createPipelines(r);
}

void DirectVideoNodeRenderer::createPipelines(RenderList& r)
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

void DirectVideoNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = renderer.defaultQuad();
  if(m_meshBuffer.buffers.empty())
  {
    m_meshBuffer = renderer.initMeshBuffer(mesh, res);
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("DirectVideoNodeRenderer::m_processUBO");
  m_processUBO->create();

  m_materialUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
  m_materialUBO->setName("DirectVideoNodeRenderer::m_materialUBO");
  m_materialUBO->create();

  // Open our own decode context
  if(!openFile())
  {
    qDebug() << "DirectVideoNodeRenderer: failed to open" << m_filePath.c_str();
  }

  createGpuDecoder();
  createPipelines(renderer);
  m_recomputeScale = true;
}

void DirectVideoNodeRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  score::gfx::quadRenderPass(renderer, m_meshBuffer, cb, edge, m_p);
}

void DirectVideoNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node().standardUBO);

  // Compute desired time in flicks
  const double currentTime = this->node().standardUBO.time;
  const int64_t currentFlicks
      = std::max(int64_t{0}, static_cast<int64_t>(currentTime * ossia::flicks_per_second<double>));

  // Only re-decode if we moved to a different time
  if(currentFlicks != m_lastRequestedFlicks)
  {
    // Frame duration in flicks
    const double fps = m_fps > 0. ? m_fps : 24.;
    const int64_t frameDurationFlicks
        = static_cast<int64_t>(ossia::flicks_per_second<double> / fps);

    // Skip decode if we already have this frame (within ±1 frame of the same position)
    const int64_t lastDecodedFlicks
        = m_lastDecodedDts == INT64_MIN
            ? INT64_MIN
            : static_cast<int64_t>(m_lastDecodedDts * m_flicks_per_dts);

    m_lastRequestedFlicks = currentFlicks;

    if(m_lastDecodedDts == INT64_MIN
       || std::abs(currentFlicks - lastDecodedFlicks) >= frameDurationFlicks)
    {
      if(seekAndDecode(currentFlicks) && m_decodedFrame && m_decodedFrame->data[0])
      {
        // Check if format changed (e.g. resolution change)
        if(m_gpu && m_useAVCodec)
        {
          const auto& n = this->node();
          auto fmt = static_cast<AVPixelFormat>(m_decodedFrame->format);
          if(fmt != m_frameFormat.pixel_format
             || m_decodedFrame->width != m_frameFormat.width
             || m_decodedFrame->height != m_frameFormat.height
             || n.m_outputFormat != m_frameFormat.output_format
             || n.m_tonemap != m_frameFormat.tonemap)
          {
            m_frameFormat.pixel_format = fmt;
            m_frameFormat.width = m_decodedFrame->width;
            m_frameFormat.height = m_decodedFrame->height;
            m_frameFormat.output_format = n.m_outputFormat;
            m_frameFormat.tonemap = n.m_tonemap;
            setupGpuDecoder(renderer);
          }
        }

        if(m_gpu)
        {
          m_gpu->exec(renderer, res, *m_decodedFrame);
        }
      }
    }
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
    mat.tex_w = m_frameFormat.width;
    mat.tex_h = m_frameFormat.height;

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }
}

void DirectVideoNodeRenderer::release(RenderList& r)
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

  closeFile();
}

}

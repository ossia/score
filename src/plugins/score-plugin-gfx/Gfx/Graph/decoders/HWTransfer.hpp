#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV444.hpp>
#include <Gfx/Graph/decoders/YUV444P10.hpp>

#include <Video/GpuFormats.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#if __has_include(<libavutil/hwcontext.h>)
#include <libavutil/hwcontext.h>
#endif
}

namespace score::gfx
{

/**
 * @brief Fallback hardware decoder that transfers HW frames to CPU via
 *        av_hwframe_transfer_data(), then delegates to a software GPU decoder.
 *
 * This avoids the software decode cost (GPU-accelerated decode) while still
 * uploading pixel data through the normal QRhi texture upload path.
 * The transfer is a GPU→CPU DMA copy, typically yielding NV12 or P010 data.
 *
 * This is the fallback used when zero-copy import is not available for the
 * current RHI backend / HW decoder combination.
 */
struct HWTransferDecoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  AVPixelFormat m_swFormat{AV_PIX_FMT_NONE};
  std::unique_ptr<GPUVideoDecoder> m_delegate;
  AVFrame* m_swFrame{};

  explicit HWTransferDecoder(
      Video::ImageFormat& d, AVPixelFormat swFormat = AV_PIX_FMT_NV12)
      : decoder{d}
      , m_swFormat{swFormat}
  {
    m_swFrame = av_frame_alloc();
  }

  static bool canDelegateFormat(AVPixelFormat fmt)
  {
    switch(fmt)
    {
      case AV_PIX_FMT_NV12:
      case AV_PIX_FMT_NV21:
      case AV_PIX_FMT_P010LE:
      case AV_PIX_FMT_YUV420P:
      case AV_PIX_FMT_YUVJ420P:
      case AV_PIX_FMT_YUV420P10LE:
      case AV_PIX_FMT_YUV422P:
      case AV_PIX_FMT_YUVJ422P:
      case AV_PIX_FMT_YUV422P10LE:
      case AV_PIX_FMT_YUV444P:
      case AV_PIX_FMT_YUVJ444P:
      case AV_PIX_FMT_YUV444P10LE:
        return true;
      default:
        return false;
    }
  }

  std::unique_ptr<GPUVideoDecoder> createDelegateForFormat(AVPixelFormat fmt)
  {
    switch(fmt)
    {
      case AV_PIX_FMT_NV12:
        return std::make_unique<NV12Decoder>(decoder, false);
      case AV_PIX_FMT_NV21:
        return std::make_unique<NV12Decoder>(decoder, true);
      case AV_PIX_FMT_P010LE:
        return std::make_unique<P010Decoder>(decoder);
      case AV_PIX_FMT_YUV420P:
      case AV_PIX_FMT_YUVJ420P:
        return std::make_unique<YUV420Decoder>(decoder);
      case AV_PIX_FMT_YUV420P10LE:
        return std::make_unique<YUV420P10Decoder>(decoder);
      case AV_PIX_FMT_YUV422P:
      case AV_PIX_FMT_YUVJ422P:
        return std::make_unique<YUV422Decoder>(decoder);
      case AV_PIX_FMT_YUV422P10LE:
        return std::make_unique<YUV422P10Decoder>(decoder);
      case AV_PIX_FMT_YUV444P:
      case AV_PIX_FMT_YUVJ444P:
        return std::make_unique<YUV444Decoder>(decoder);
      case AV_PIX_FMT_YUV444P10LE:
        return std::make_unique<YUV444P10Decoder>(decoder);
      default:
        return std::make_unique<NV12Decoder>(decoder, false);
    }
  }

  ~HWTransferDecoder() override
  {
    if(m_swFrame)
      av_frame_free(&m_swFrame);

    // Clear delegate's samplers to prevent double-free:
    // our samplers vector shares the same pointers as the delegate's,
    // and the base class GPUVideoDecoder::release() already cleaned them.
    if(m_delegate)
      m_delegate->samplers.clear();
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    if(!m_delegate)
    {
      decoder.pixel_format = m_swFormat;
      m_delegate = createDelegateForFormat(m_swFormat);
    }

    auto ret = m_delegate->init(r);
    samplers = m_delegate->samplers;
    return ret;
  }

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if(!Video::formatIsHardwareDecoded(static_cast<AVPixelFormat>(frame.format)))
    {
      // Already a software frame (e.g. after format change), delegate directly
      if(m_delegate)
        m_delegate->exec(r, res, frame);
      return;
    }

    // Transfer HW frame → CPU.
    // If the native sw_format is one we can't directly render (e.g. AYUV from
    // ProRes 4444+alpha), request a format we handle instead. FFmpeg will
    // convert internally during the transfer.
    av_frame_unref(m_swFrame);
    if(canDelegateFormat(m_swFormat))
      m_swFrame->format = AV_PIX_FMT_NONE; // Use native sw_format
    else
      m_swFrame->format = AV_PIX_FMT_NV12; // Request a supported fallback
    int ret = av_hwframe_transfer_data(m_swFrame, &frame, 0);
    if(ret < 0)
    {
      qDebug() << "HWTransferDecoder: av_hwframe_transfer_data failed:" << ret;
      return;
    }
    m_swFrame->pts = frame.pts;
    m_swFrame->pkt_dts = frame.pkt_dts;

    auto sw_fmt = static_cast<AVPixelFormat>(m_swFrame->format);

    // If the software format changed (first frame, or dynamic change), rebuild delegate
    if(sw_fmt != m_swFormat)
    {
      m_swFormat = sw_fmt;
      decoder.pixel_format = sw_fmt;
      decoder.width = m_swFrame->width;
      decoder.height = m_swFrame->height;

      // Format changed — rebuild delegate with correct textures/shaders.
      // This should rarely happen since we pre-set sw_format at construction.
      if(m_delegate)
      {
        m_delegate->samplers.clear();
        m_delegate.reset();
      }
      samplers.clear();

      m_delegate = createDelegateForFormat(sw_fmt);
      if(m_delegate)
      {
        m_delegate->init(r);
        samplers = m_delegate->samplers;
      }
    }

    if(m_delegate)
      m_delegate->exec(r, res, *m_swFrame);
#endif
  }
};

}

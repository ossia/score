#include <Media/Libav.hpp>

#include "VideoDecoder.hpp"

#include <iostream>
#include <QTimer>
#include <QApplication>

#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>

#include <QDebug>
#include <QElapsedTimer>
#include <thread>

#include <functional>

#if SCORE_HAS_LIBAV

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#if __APPLE__ && __has_include(<libavcodec/videotoolbox.h>)
#include "VideoDecoder.vtb.cpp"
#endif

namespace Video
{
void LibAVDecoder::init_scaler(VideoInterface& self) noexcept
{
  if(!Video::formatNeedsDecoding(self.pixel_format))
    return;

  m_rescale.open(self);
  self.pixel_format = AV_PIX_FMT_RGBA;
}

bool LibAVDecoder::open_codec_context(
    VideoInterface& self, const AVStream* stream,
    std::function<void(AVCodecContext&)> setup)
{
  m_codecContext = avcodec_alloc_context3(m_codec);
  m_codecContext->hwaccel_context = nullptr;

  avcodec_parameters_to_context(m_codecContext, stream->codecpar);

#if LIBAVUTIL_VERSION_MAJOR >= 57
  auto [hw_dev_ctx, hw_codec] = open_hwdec(*m_codec);
  if(hw_dev_ctx)
  {
    m_codecContext->hw_device_ctx = hw_dev_ctx;

    m_codecContext->get_format = +[](AVCodecContext* ctx, const AVPixelFormat* p) {
      while(*p != AV_PIX_FMT_NONE)
      {
        auto fmt = ffmpegHardwareDecodingFormats(*p).format;
        if(fmt != AV_PIX_FMT_NONE)
        {
          return fmt;
        }
        ++p;
      }

      return ctx->pix_fmt;
    };
  }
#endif

  // m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
  // m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
  #if defined(__APPLE__)
  if(hw_dev_ctx)
  {
    m_codecContext->thread_count = 1;
    m_codecContext->thread_type = FF_THREAD_SLICE;
  }
  else
  #endif
  {
    m_codecContext->thread_count = m_conf.threads;
    if(m_conf.threads > 0)
      m_codecContext->thread_type = FF_THREAD_SLICE;
  }

  SCORE_ASSERT(setup);
  setup(*m_codecContext);

  bool res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);

  if(res)
    init_scaler(self);
  return res;
}

/*
 *
    using codec_map_type = std::map<AVCodecID, const char*>;
    static const codec_map_type codecs{
        {AV_CODEC_ID_AV1, "av1_cuvid"},          {AV_CODEC_ID_H264, "h264_cuvid"},
        {AV_CODEC_ID_HEVC, "hevc_cuvid"},        {AV_CODEC_ID_MJPEG, "mjpeg_cuvid"},
        {AV_CODEC_ID_MPEG1VIDEO, "mpeg1_cuvid"}, {AV_CODEC_ID_MPEG2VIDEO, "mpeg2_cuvid"},
        {AV_CODEC_ID_MPEG4, "mpeg4_cuvid"},      {AV_CODEC_ID_VC1, "vc1_cuvid"},
        {AV_CODEC_ID_VP8, "vp8_cuvid"},          {AV_CODEC_ID_VP9, "vp9_cuvid"},
    };
    */
static std::string hwCodecMap(std::string name, AVHWDeviceType device)
{
  switch(device)
  {
    case AV_HWDEVICE_TYPE_CUDA:
      return name + "_cuvid";
    case AV_HWDEVICE_TYPE_QSV:
      return name + "_qsv";
    case AV_HWDEVICE_TYPE_VDPAU:
      return name + "_vdpau";
    case AV_HWDEVICE_TYPE_VAAPI:
      return name + "_vaapi";
    case AV_HWDEVICE_TYPE_DXVA2:
      return name + "_dxva2";
    case AV_HWDEVICE_TYPE_D3D11VA:
      return name + "_d3d11va2";
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
      return name;
    default:
      return {};
  }
}
std::pair<AVBufferRef*, const AVCodec*>
LibAVDecoder::open_hwdec(const AVCodec& detected_codec) noexcept
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  if(m_conf.hardwareAcceleration == AV_PIX_FMT_NONE)
    return {};

  const auto device = ffmpegHardwareDecodingFormats(m_conf.hardwareAcceleration).device;
  if(device == AV_HWDEVICE_TYPE_NONE)
    return {};

  // Look for a correct codec
  auto mapped = hwCodecMap(detected_codec.name, device);
  if(mapped.empty())
    return {};

  auto codec = mapped == detected_codec.name
                   ? &detected_codec // VideoToolbox case
                   : avcodec_find_decoder_by_name(mapped.c_str());
  if(!codec)
    return {};

  AVBufferRef* hw_device_ctx{};
  int ret = av_hwdevice_ctx_create(&hw_device_ctx, device, nullptr, nullptr, 0);
  if(ret != 0)
    return {};

  return {hw_device_ctx, codec};
#else
  return {};
#endif
}

ReadFrame LibAVDecoder::enqueue_frame(const AVPacket* pkt) noexcept
{
  auto frame = m_frames.newFrame();

  ReadFrame read = readVideoFrame(m_codecContext, pkt, frame.get());
  if(read.error == AVERROR_EOF)
  {
    m_finished = true;
  }

  if(!read.frame)
  {
    this->m_frames.enqueue_decoding_error(frame.release());
    return read;
  }

  if(m_rescale)
  {
    m_rescale.rescale(m_frames, frame, read);
  }
  else
  {
    // it is already stored in "read" but well
    if(read.frame == frame.get())
      frame.release();
  }
  return read;
}

static void listHardwareDecodeTextureFormats(AVFrame* frame)
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  AVPixelFormat* arr = {};
  av_hwframe_transfer_get_formats(
      frame->hw_frames_ctx,
      AVHWFrameTransferDirection::AV_HWFRAME_TRANSFER_DIRECTION_FROM, &arr, 0);
  for(auto p = arr; *p != AV_PIX_FMT_NONE; ++p)
  {
    auto desc = av_pix_fmt_desc_get(*p);
    if(desc)
      qDebug() << "supported format : " << desc->name;
  }
  av_free(arr);
#endif
}

// Mainly used for HAP which we do not want to decode through ffmpeg
void LibAVDecoder::load_packet_in_frame(const AVPacket& packet, AVFrame& frame)
{
  auto cp = m_avstream->codecpar;
  // TODO this is a hack, we store the FOURCC in the format...

  memcpy(&frame.format, &cp->codec_tag, 4);

  frame.buf[0] = av_buffer_ref(packet.buf);
  frame.width = cp->width;
  frame.height = cp->height;
  frame.format = (cp->codec_tag);
  frame.best_effort_timestamp = packet.pts;
  frame.data[0] = packet.data;
  frame.linesize[0] = packet.size;
  frame.pts = packet.pts;
  frame.pkt_dts = packet.dts;
  frame.pkt_duration = packet.duration;
}

ReadFrame
readVideoFrame(AVCodecContext* codecContext, const AVPacket* pkt, AVFrame* frame)
{
  if(codecContext && pkt && frame)
  {
    int ret = avcodec_send_packet(codecContext, pkt);
    if(ret < 0)
    {
      if(ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
      {
        qDebug() << "avcodec_send_packet: " << av_to_string(ret) << ret;
      }
      return {nullptr, ret};
    }

    ret = avcodec_receive_frame(codecContext, frame);

    if(ret < 0)
    {
      return {nullptr, ret};
    }
    else
    {
      if(frame->pts >= 0)
      {
#if LIBAVUTIL_VERSION_MAJOR >= 57
        // Process hardware acceleration
        if(formatIsHardwareDecoded(AVPixelFormat(frame->format)))
        {
          AVFrame* sw_frame = av_frame_alloc();
          sw_frame->width = frame->width;
          sw_frame->height = frame->height;
          sw_frame->format = AV_PIX_FMT_NONE;

          av_hwframe_transfer_data(sw_frame, frame, 0);
          sw_frame->pts = frame->pts;

          return {sw_frame, ret};
        }
#endif
        return {frame, ret};
      }
      else
      {
        return {nullptr, ret};
      }
    }
  }

  return {nullptr, AVERROR_UNKNOWN};
}

VideoInterface::~VideoInterface() { }

VideoDecoder::VideoDecoder(DecoderConfiguration conf) noexcept
{
  m_conf = std::move(conf);
}

VideoDecoder::~VideoDecoder() noexcept
{
  close_file();
}

bool VideoDecoder::open(const std::string& inputFile) noexcept
{
  close_file();

  m_inputFile = inputFile;

  if(avformat_open_input(&m_formatContext, inputFile.c_str(), nullptr, nullptr) != 0)
  {
    close_file();
    return false;
  }

  if(avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    close_file();
    return false;
  }

  if(!open_stream())
  {
    close_file();
    return false;
  }

  int64_t secs = m_formatContext->duration / AV_TIME_BASE;
  int64_t us = m_formatContext->duration % AV_TIME_BASE;

  m_duration = secs * ossia::flicks_per_second<int64_t>;
  m_duration += us * ossia::flicks_per_millisecond<int64_t> / 1000;

  return true;
}

bool VideoDecoder::load(const std::string& inputFile) noexcept
{
  if(!open(inputFile))
    return false;

  m_running.store(true, std::memory_order_release);
  // TODO use a thread pool
  m_thread = std::thread{[this] { this->buffer_thread(); }};

  return true;
}

int64_t VideoDecoder::duration() const noexcept
{
  return m_duration;
}

void VideoDecoder::seek(int64_t flicks)
{
  m_seekTo = flicks;
}

AVFrame* VideoDecoder::dequeue_frame() noexcept
{
  auto f = m_frames.discard_and_dequeue_one();
  if(f)
  {
    m_last_dequeued_dts = f->pkt_dts;
  }
  m_condVar.notify_one();
  return f;
}

void VideoDecoder::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}

void VideoDecoder::buffer_thread() noexcept
{
  while(m_running.load(std::memory_order_acquire))
  {
    if(int64_t seek = m_seekTo.exchange(-1); seek >= 0)
    {
      seek_impl(seek);
    }
    else
    {
      std::unique_lock lck{m_condMut};
      m_condVar.wait(lck, [&] {
        return (m_frames.size() < frames_to_buffer / 2 && !m_finished)
               || !m_running.load(std::memory_order_acquire) || (m_seekTo != -1);
      });
      if(!m_running.load(std::memory_order_acquire))
        return;

      if(int64_t seek = m_seekTo.exchange(-1); seek >= 0)
      {
        seek_impl(seek);
      }

      if(m_frames.size() < (frames_to_buffer / 2) && !m_finished)
      {
        if(auto f = read_frame_impl())
        {
          m_frames.enqueue(f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
      }
    }
  }
}

void VideoDecoder::close_file() noexcept
{
  // Stop the running status
  m_running.store(false, std::memory_order_release);
  m_condVar.notify_one();

  if(m_thread.joinable())
    m_thread.join();

  // Clear the stream
  close_video();

  // Clear the fmt context
  if(m_formatContext)
  {
    avio_flush(m_formatContext->pb);
    avformat_flush(m_formatContext);
    avformat_close_input(&m_formatContext);
    avformat_free_context(m_formatContext);
    m_formatContext = nullptr;
  }

  // Remove frames that were in flight
  m_frames.drain();
}

ReadFrame LibAVDecoder::read_one_frame_raw(AVPacket& packet)
{
  auto frame = m_frames.newFrame();
  int res{};
  if(frame->buf[0])
    av_buffer_unref(&frame->buf[0]);

  while((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if(packet.stream_index == m_avstream->index)
    {
      // Mainly for HAP: we feed the raw undecoded codec data directly to the GPU, see HAPDecoder
      load_packet_in_frame(packet, *frame);

      av_packet_unref(&packet);
      return {frame.release(), 0};
    }
    else
    {
      av_packet_unref(&packet);
    }
  }

  if(res != 0 && res != AVERROR_EOF)
  {
    // qDebug() << "Error while reading a frame: "
    //          << av_to_string(res);
  }
  else if(res == AVERROR_EOF)
  {
    m_finished = true;
  }
  av_packet_unref(&packet);
  return {nullptr, res};
}

ReadFrame LibAVDecoder::read_one_frame_avcodec(AVPacket& packet)
{
  int res{};
  av_packet_unref(&packet);
  while((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if(packet.stream_index == m_avstream->index)
    {
      SCORE_ASSERT(m_codecContext);

      av_packet_rescale_ts(
          &packet, this->m_avstream->time_base, this->m_codecContext->time_base);

      auto ret_frame = enqueue_frame(&packet);

      av_packet_unref(&packet);
      return ret_frame;
    }
    else
    {
      av_packet_unref(&packet);
    }
  }

  if(res != 0 && res != AVERROR_EOF)
  {
    // qDebug() << "Error while reading a frame: "
    //          << av_to_string(res);
  }
  else if(res == AVERROR_EOF)
  {
    m_finished = true;
  }
  av_packet_unref(&packet);
  return {nullptr, res};
}

ReadFrame LibAVDecoder::read_one_frame(AVPacket& packet)
{
  if(m_conf.useAVCodec)
    return read_one_frame_avcodec(packet);
  else
    return read_one_frame_raw(packet);
}
/*
// https://stackoverflow.com/a/44468529/1495627
static
int seek_to_frame(AVFormatContext* format, AVStream* stream, int frameIndex)
{
  using namespace std;
  // Seek is done on packet dts
  int64_t target_dts_usecs = std::round(frameIndex * (double)stream->r_frame_rate.den / stream->r_frame_rate.num * AV_TIME_BASE);
  // Remove first dts: when non zero seek should be more accurate
  auto first_dts_usecs = std::round(stream->first_dts * (double)stream->time_base.num / stream->time_base.den * AV_TIME_BASE);
  target_dts_usecs += first_dts_usecs;
  return av_seek_frame(format, -1, target_dts_usecs, AVSEEK_FLAG_BACKWARD);
}
*/

static int64_t to_av_time_base(AVRational tb, int64_t dts)
{
  constexpr auto av_tb = AVRational{1, AV_TIME_BASE};
  return av_rescale_q(dts, tb, av_tb);
}

bool VideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if(m_avstream->index >= int(m_formatContext->nb_streams))
    return false;

  // Seeking with stream == -1 means that it is done AV_TIME_BASE
  constexpr auto av_tb = AVRational{1, AV_TIME_BASE};
  constexpr auto av_dts_per_flicks
      = (av_tb.den / (av_tb.num * ossia::flicks_per_second<double>));

  const int64_t dts = flicks * av_dts_per_flicks;

  const auto codec_tb
      = m_codecContext ? m_codecContext->time_base : m_avstream->time_base;

  // Don't seek if we're less than 0.2 second close to the request
  // unit of the timestamps in seconds: stream->time_base.num / stream->time_base.den

  // qDebug() << "Codec pkt_timebase: " << m_codecContext->pkt_timebase.num
  //          << m_codecContext->pkt_timebase.den;
  // qDebug() << "Codec timebase: " << m_codecContext->time_base.num
  //          << m_codecContext->time_base.den;
  // qDebug() << "Stream timebase: " << stream->time_base.num << stream->time_base.den;
  // qDebug() << "AV timebase: " << av_tb.num << av_tb.den;
  const auto last_av_dts = to_av_time_base(codec_tb, m_last_dequeued_dts);
  // qDebug() << "????" << m_last_dequeued_dts << last_av_dts;
  const int64_t min_dts_delta = (0.2 * av_tb.den) / av_tb.num;
  // qDebug() << AV_TIME_BASE << min_dts_delta << dts << last_av_dts << dts - last_av_dts
  //          << (std::abs(dts - last_av_dts) <= min_dts_delta);
  if(std::abs(dts - last_av_dts) <= min_dts_delta)
  {
    // Let's always ensure that we seek to zero when asked no matter what
    if(dts != 0)
    {
      return false;
    }
  }

  // TODO - maybe we should also store the "last dequeued dts" from the
  // decoder side - this way no need to seek if we are in the interval
  // const bool seek_forward = dts >= this->m_last_dequeued_dts;
#if LIBAVFORMAT_VERSION_MAJOR >= 59
  const int64_t start = 0;
#else
  const int64_t start = m_avstream->first_dts;
#endif

  if(!ossia::seek_to_flick(m_formatContext, m_codecContext, m_avstream, flicks))
  {
    qDebug() << "Failed to seek for time ";
    return false;
  }

  ReadFrame r;
  do
  {
    // First flush the buffer or smth
    do
    {
      if(r.frame)
      {
        av_frame_free(&r.frame);
      }

      auto pkt = av_packet_alloc();
      r = read_one_frame(*pkt);
      av_packet_unref(pkt);
      av_packet_free(&pkt);
    } while(r.error == AVERROR(EAGAIN));

    if(r.error == AVERROR_EOF || !r.frame)
    {
      break;
    }

    /*
    // Rescale the packet's dts into AV_TIME_BASE
    auto max_dts = r.frame->pkt_dts + r.frame->pkt_duration;
    auto max_av_dts = to_av_time_base(codec_tb, max_dts);
    //av_rescale_q(max_dts, stream->time_base, tb);
    // we're starting to see correct frames, try to get close to the dts we want.
    while(max_av_dts < dts)
    {
      r = read_one_frame(AVFramePointer{r.frame}, pkt);
      if(r.error == AVERROR_EOF || !r.frame)
        break;
    }
    */
  } while(0);

  if(r.frame)
  {
    m_frames.set_discard_frame(r.frame);
    m_frames.enqueue(r.frame);
  }
  else
  {
    av_frame_free(&r.frame);
  }

  m_finished = false;

  return true;
}

AVFrame* VideoDecoder::read_frame_impl() noexcept
{
  ReadFrame res;

  if(m_avstream)
  {
    auto packet = av_packet_alloc();

    do
    {
      av_packet_unref(packet);
      res = read_one_frame(*packet);

      if(res.error == AVERROR_EOF)
      {
        m_finished = true;
        av_packet_unref(packet);
        av_packet_free(&packet);
        return res.frame;
      }
    } while(res.error == AVERROR(EAGAIN));

    av_packet_unref(packet);
    av_packet_free(&packet);
  }
  return res.frame;
}

bool VideoDecoder::open_stream() noexcept
{
  bool res = false;

  if(!m_formatContext)
    return res;

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

  if(stream != -1)
  {
    m_avstream = m_formatContext->streams[stream];
    const AVRational tb = m_avstream->time_base;
    dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
    flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

    auto codecPar = m_avstream->codecpar;
    if((m_codec = avcodec_find_decoder(codecPar->codec_id)))
    {
      if(codecPar->width <= 0 || codecPar->height <= 0)
      {
        qDebug() << "VideoDecoder: invalid video: width or height is 0";
        res = false;
      }
      else
      {
        if(m_avstream->codecpar->codec_id == AV_CODEC_ID_HAP)
        {
          // TODO this is a hack, we store the FOURCC in the format...
          memcpy(&pixel_format, &m_avstream->codecpar->codec_tag, 4);
          width = codecPar->width;
          height = codecPar->height;
          fps = av_q2d(m_avstream->avg_frame_rate);

          m_conf.useAVCodec = false;
          m_codecContext = nullptr;
          m_codec = nullptr;
          res = true;
        }
        else
        {
          pixel_format = (AVPixelFormat)codecPar->format;
          width = codecPar->width;
          height = codecPar->height;
          fps = av_q2d(m_avstream->avg_frame_rate);

          res = open_codec_context(*this, m_avstream, [=](AVCodecContext& ctx) {
            ctx.framerate
                 = av_guess_frame_rate(m_formatContext, (AVStream*)m_avstream, NULL);
            m_codecContext->pkt_timebase = m_avstream->time_base;
            // m_codecContext->codec_id = m_codec->id;
          });

          if(m_codecContext)
          {
            auto tb = m_codecContext->time_base;
            dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
            flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;
          }
        }
      }
    }
  }

  if(!res)
  {
    close_video();
  }
  return res;
}

void VideoDecoder::close_video() noexcept
{
  if(m_codecContext)
  {
    avcodec_flush_buffers(m_codecContext);
    if(m_codecContext->hwaccel_context)
      av_videotoolbox_default_free(m_codecContext);
    avcodec_free_context(&m_codecContext);

    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  m_rescale.close();

  m_avstream = nullptr;
}
}
#endif

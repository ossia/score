#include "VideoDecoder.hpp"

#include <Media/Libav.hpp>
#include <Video/FrameAccess.hpp>
#include <Video/GpuFormats.hpp>
#include <Video/PlaybackTime.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

#if SCORE_HAS_LIBAV

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavcodec/packet.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
#if __has_include(<libavutil/mastering_display_metadata.h>)
#include <libavutil/mastering_display_metadata.h>
#endif
#endif
}

#if __APPLE__ && __has_include(<libavcodec/videotoolbox.h>)
#include "VideoDecoder.vtb.cpp"
#endif

namespace Video
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
static auto get_format_for_codeccontext(AVCodecContext* ctx, const AVPixelFormat* p)
{
  //qDebug() << "device: " << av_pix_fmt_desc_get(ctx->pix_fmt)->name;

  if(auto self = (LibAVDecoder*)ctx->opaque)
  {
    while(*p != AV_PIX_FMT_NONE)
    {
      //qDebug() << av_pix_fmt_desc_get(*p)->name;
      // Check if the format matches the one we want from the expected HWDec
      if(*p == self->m_conf.hardwareAcceleration)
      {
        // Check if the format is indeed available
        auto fmt = ffmpegHardwareDecodingFormats(*p).format;
        if(fmt != AV_PIX_FMT_NONE)
        {
          return fmt;
        }
      }
      ++p;
    }
  }

  return ctx->pix_fmt;
}
#endif

void LibAVDecoder::init_scaler(VideoInterface& self) noexcept
{
  if(!Video::formatNeedsDecoding(self.pixel_format))
    return;

  m_rescale.open(self);

  // Only claim RGBA once the rescaler that produces it actually opened:
  // sws_getContext refuses some source descriptions (a stream whose
  // codecpar->format is AV_PIX_FMT_NONE, for one), and the renderer copies
  // color_space out of this metadata and applies it to frames that are still
  // in their native YUV.
  if(!m_rescale)
    return;

  self.pixel_format = AV_PIX_FMT_RGBA;
  self.color_space = AVCOL_SPC_RGB;
}

int LibAVDecoder::init_codec_context(
    const AVCodec* codec, AVBufferRef* hw_dev_ctx, const AVStream* stream,
    std::function<void(AVCodecContext&)> setup)
{
  m_codecContext = avcodec_alloc_context3(codec);

  avcodec_parameters_to_context(m_codecContext, stream->codecpar);

  // m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
  // m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
  const auto threading = chooseDecoderThreading(
      *codec, stream->codecpar, m_conf.useCase, m_conf.threads, hw_dev_ctx != nullptr);
#if LIBAVUTIL_VERSION_MAJOR >= 57
  if(hw_dev_ctx)
  {
    m_codecContext->hw_device_ctx = hw_dev_ctx;
    m_codecContext->opaque = (void*)this;
    m_codecContext->get_format = get_format_for_codeccontext;
  }
#endif
  applyDecoderThreading(*m_codecContext, threading);

  SCORE_ASSERT(setup);
  setup(*m_codecContext);

  int err = avcodec_open2(m_codecContext, codec, nullptr);
  if(err < 0)
  {
    qDebug() << "avcodec_open2: " << av_to_string(err);
    avcodec_free_context(&m_codecContext);
  }
  else
  {
    qDebug().noquote() << "Video decoder:"
                       << describeDecoderThreading(*m_codecContext, threading).c_str();
  }
  return err;
}

bool LibAVDecoder::open_codec_context(
    VideoInterface& self, const AVStream* stream,
    std::function<void(AVCodecContext&)> setup)
{
  if(auto [hw_dev_ctx, hw_codec] = open_hwdec(*m_codec); hw_codec)
  {
    int err = init_codec_context(hw_codec, hw_dev_ctx, stream, setup);
    if(err == 0)
    {
      init_scaler(self);
      return true;
    }
  }

  // Maybe opening an HW accel failed, we retry in software mode
  int err = init_codec_context(m_codec, nullptr, stream, setup);
  if(err == 0)
  {
    init_scaler(self);
    return true;
  }
  return false;
}

/*
 *
    using codec_map_type = ossia::flat_map<AVCodecID, const char*>;
    static const codec_map_type codecs{
        {AV_CODEC_ID_AV1, "av1_cuvid"},          {AV_CODEC_ID_H264, "h264_cuvid"},
        {AV_CODEC_ID_HEVC, "hevc_cuvid"},        {AV_CODEC_ID_MJPEG, "mjpeg_cuvid"},
        {AV_CODEC_ID_MPEG1VIDEO, "mpeg1_cuvid"}, {AV_CODEC_ID_MPEG2VIDEO, "mpeg2_cuvid"},
        {AV_CODEC_ID_MPEG4, "mpeg4_cuvid"},      {AV_CODEC_ID_VC1, "vc1_cuvid"},
        {AV_CODEC_ID_VP8, "vp8_cuvid"},          {AV_CODEC_ID_VP9, "vp9_cuvid"},
    };
    */
std::pair<AVBufferRef*, const AVCodec*>
LibAVDecoder::open_hwdec(const AVCodec& detected_codec) noexcept
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  auto hwAccel = m_conf.hardwareAcceleration;
  if(hwAccel == AV_PIX_FMT_NONE)
    return {};

  if(hwAccel == AV_PIX_FMT_NONE
     || !codecSupportsHWPixelFormat(detected_codec.id, hwAccel))
  {
    auto autoFmt = selectHardwareAcceleration(
        m_conf.graphicsApi, detected_codec.id, m_conf.gpuVendorId);
    if(autoFmt != AV_PIX_FMT_NONE)
      hwAccel = autoFmt;
    else
      return {};
  }

  const auto device = ffmpegHardwareDecodingFormats(hwAccel).device;
  if(device == AV_HWDEVICE_TYPE_NONE)
    return {};

  auto mapped = Video::hwCodecName(detected_codec.name, device);
  if(mapped.empty())
    return {};

  auto codec = mapped == detected_codec.name
                   ? &detected_codec // VideoToolbox case
                   : avcodec_find_decoder_by_name(mapped.c_str());
  if(!codec)
    return {};

  if(hwAccel == AV_PIX_FMT_DRM_PRIME)
  {
    // V4L2M2M: just want to map h264 to h264_v4l2m2m,
    // this isn't a true "hwdevice" accel
    return {nullptr, codec};
  }

  AVBufferRef* hw_device_ctx{};
  int ret = Video::createHardwareDevice(&hw_device_ctx, device);
  if(ret != 0)
    return {};

  if(hwAccel == AV_PIX_FMT_QSV)
    return {hw_device_ctx, codec};
  else
    return {hw_device_ctx, &detected_codec};
#else
  return {};
#endif
}

ReadFrame LibAVDecoder::enqueue_frame(const AVPacket* pkt) noexcept
{
  auto receive = [this]() -> ReadFrame
  {
    auto frame = m_frames.newFrame();
    auto read
        = receiveVideoFrame(
            m_codecContext, frame.get(), this->m_conf.ignorePTS, &m_missingTimestamps);
    if(read.error == AVERROR_EOF)
      m_finished = true;

    if(!read.frame)
    {
      m_frames.enqueue_decoding_error(frame.release());
      return read;
    }

    if(m_rescale)
    {
      m_rescale.rescale(m_frames, frame, read);
    }
    else if(read.frame == frame.get())
    {
      frame.release();
    }

    return read;
  };

  ReadFrame last{nullptr, AVERROR(EAGAIN)};
  auto keepInOrder = [this, &last](ReadFrame read) {
    if(last.frame)
      m_frames.enqueue(last.frame);
    last = read;
  };

  // A decoder may refuse a new packet until every pending frame has been
  // received. Drain those frames and retry the exact same packet.
  for(;;)
  {
    const int ret = avcodec_send_packet(m_codecContext, pkt);
    if(ret == 0)
      break;
    if(ret != AVERROR(EAGAIN))
    {
      if(ret == AVERROR_EOF)
        m_finished = true;
      else
        qDebug() << "avcodec_send_packet: " << av_to_string(ret) << ret;
      return last.frame ? last : ReadFrame{nullptr, ret};
    }

    auto read = receive();
    if(read.frame)
    {
      keepInOrder(read);
      continue;
    }

    // error == 0 with no frame: a frame was decoded but discarded (negative
    // pts). The codec still made room, so the packet must be retried, not
    // dropped.
    if(read.error == 0)
      continue;

    // EAGAIN from send_packet guarantees receive_frame yields a frame; if it
    // does not, no progress is possible: bail out instead of spinning.
    return last.frame ? last : read;
  }

  // One packet can make more than one frame available. Receive all of them
  // before reading the next packet so inter-frame reference chains stay intact.
  for(;;)
  {
    auto read = receive();
    if(read.frame)
    {
      keepInOrder(read);
      continue;
    }
    // A decoded-but-discarded frame (negative pts) is not the end of the
    // available frames: keep draining.
    if(read.error == 0)
      continue;
    if(read.error != AVERROR(EAGAIN) && read.error != AVERROR_EOF)
      return last.frame ? last : read;
    break;
  }

  if(last.frame)
    last.error = 0;
  return last;
}

#if 0
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
#endif

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
#if(LIBAVUTIL_VERSION_MAJOR < 58)
  frame.pkt_duration = packet.duration;
#else
  frame.duration = packet.duration;
#endif
}

ReadFrame receiveVideoFrame(
    AVCodecContext* codecContext, AVFrame* frame, bool ignorePts,
    MissingTimestamps* missing)
{
  if(codecContext && frame)
  {
    int ret = avcodec_receive_frame(codecContext, frame);

    if(ret < 0)
    {
      return {nullptr, ret};
    }
    else
    {
      if(missing)
      {
        if(frame->pts == AV_NOPTS_VALUE)
          frame->pts = frame->best_effort_timestamp;
        // Without timestamps the demuxer's durations are guesses too: raw
        // H.264 ones fall to 1 tick after a byte seek. Number by the frame
        // rate.
        int64_t duration = missing->step;
        if(frame->pts == AV_NOPTS_VALUE)
        {
          frame->pts = missing->next;
#if(LIBAVUTIL_VERSION_MAJOR < 58)
          frame->pkt_duration = duration;
#else
          frame->duration = duration;
#endif
        }
        else
#if(LIBAVUTIL_VERSION_MAJOR < 58)
          duration = frame->pkt_duration > 0 ? frame->pkt_duration : missing->step;
#else
          duration = frame->duration > 0 ? frame->duration : missing->step;
#endif
        missing->next = frame->pts + duration;
      }

      if(ignorePts || frame->pts >= 0)
      {
#if LIBAVUTIL_VERSION_MAJOR >= 57
        // Transfer HW frame to CPU
        if(formatIsHardwareDecoded(AVPixelFormat(frame->format)))
        {
          AVFrame* sw_frame = av_frame_alloc();
          sw_frame->format = AV_PIX_FMT_NONE;

          int hw_ret = av_hwframe_transfer_data(sw_frame, frame, 0);
          if(hw_ret >= 0)
          {
            sw_frame->pts = frame->pts;
            av_frame_unref(frame);
            av_frame_move_ref(frame, sw_frame);
          }
          av_frame_free(&sw_frame);
          if(hw_ret < 0)
            return {nullptr, hw_ret};
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
  this->filePath = inputFile;

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

  // Elementary streams and truncated files have no known duration:
  // formatContext->duration stays AV_NOPTS_VALUE (INT64_MIN), and scaling
  // that to flicks is a signed overflow. Report 0, like "unknown".
  if(m_formatContext->duration == AV_NOPTS_VALUE || m_formatContext->duration < 0)
  {
    m_duration = 0;
  }
  else
  {
    int64_t secs = m_formatContext->duration / AV_TIME_BASE;
    int64_t us = m_formatContext->duration % AV_TIME_BASE;

    m_duration = secs * ossia::flicks_per_second<int64_t>;
    m_duration += us * ossia::flicks_per_millisecond<int64_t> / 1000;
  }

  return true;
}

bool VideoDecoder::load(const std::string& inputFile) noexcept
{
  if(!open(inputFile))
    return false;

  m_running.store(true, std::memory_order_release);
  // TODO use a thread pool
  m_thread = std::thread{[this] {
    ossia::set_thread_name("ossia video");
    this->buffer_thread();
  }};

  return true;
}

int64_t VideoDecoder::duration() const noexcept
{
  return m_duration;
}

void VideoDecoder::seek(int64_t flicks)
{
  m_seekTo = flicks;
  m_condVar.notify_one();
}

AVFrame* VideoDecoder::dequeue_frame() noexcept
{
  auto f = m_frames.discard_and_dequeue_one();
  if(f)
  {
    m_last_dequeued_pts = f->pts;
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

  // Remove frames that were in flight
  m_frames.drain();

  // Clear the stream
  close_video();

  // Clear the fmt context
  if(m_formatContext)
  {
    // avformat_close_input() already frees the context and sets it to nullptr;
    // do NOT also call avformat_free_context() on it (double free).
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }
}

ReadFrame LibAVDecoder::read_one_frame_raw(AVPacket& packet)
{
  int res{};

  while((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if(packet.stream_index == m_avstream->index)
    {
      auto frame = m_frames.newFrame();
      if(frame->buf[0])
        av_buffer_unref(&frame->buf[0]);
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

  // A file that ends in garbage (truncation, an interrupted transcode) makes
  // av_read_frame return AVERROR_INVALIDDATA & co forever instead of EOF.
  // Either way no more packets will ever come: finish, like ffplay does —
  // otherwise buffer_thread retries this error at full tilt for the rest of
  // the process's life.
  if(res < 0 && res != AVERROR(EAGAIN))
  {
    m_finished = true;
  }
  av_packet_unref(&packet);
  return {nullptr, res};
}

ReadFrame LibAVDecoder::read_one_frame_avcodec(AVPacket& packet)
{
  ReadFrame ret_frame;
  int res{};

  int z = 0;
do_read_frame:
  av_packet_unref(&packet);
  while((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if(packet.stream_index == m_avstream->index)
    {
      SCORE_ASSERT(m_codecContext);
      if(packet.pts != AV_NOPTS_VALUE && packet.dts != AV_NOPTS_VALUE)
        m_ptsLead = std::max(m_ptsLead, packet.pts - packet.dts);

      //av_packet_rescale_ts(
      //     &packet, this->m_avstream->time_base, this->m_codecContext->pkt_timebase);

      ret_frame = enqueue_frame(&packet);
      if(ret_frame.error == AVERROR(EAGAIN))
      {
        if(z++ < 100)
          goto do_read_frame;
      }
      av_packet_unref(&packet);
      return ret_frame;
    }
    else
    {
      av_packet_unref(&packet);
    }
  }

  // AVERROR_EOF, or a persistent demux error: a truncated or corrupt tail
  // makes av_read_frame return AVERROR_INVALIDDATA & co forever, never EOF.
  // Either way no more packets will ever come, so both are the end of the
  // stream, like ffplay treats them — otherwise buffer_thread retries the
  // error at full tilt forever and the frames still buffered inside the
  // decoder are never shown.
  if(res < 0 && res != AVERROR(EAGAIN))
  {
    // Flush codec to get remaining frames from the reorder buffer (B-frames)
    if(m_codecContext)
    {
      // enqueue_frame routes every drained frame through the rescaler, like
      // every other decode path: these are the last frames of the clip, and a
      // decoder whose pixel_format was relabelled RGBA by init_scaler must
      // not suddenly emit its native format for them.
      auto flushed = enqueue_frame(nullptr);
      if(flushed.frame)
        m_frames.enqueue(flushed.frame);
    }
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

bool VideoDecoder::seek_impl(int64_t flicks) noexcept
{
  if(m_avstream->index >= int(m_formatContext->nb_streams))
    return false;

  // `flicks` is playback time; the stream's timestamps start at start_pts.
  const AVRational stream_tb = m_avstream->time_base;
  const int64_t target = flicksToPts(flicks, stream_tb, start_pts);
  const int64_t absolute_flicks
      = flicks + av_rescale_q(start_pts, stream_tb, flicks_time_base);

  constexpr auto av_tb = AVRational{1, AV_TIME_BASE};

  // No seek for a target less than 0.2 s ahead of the last frame taken: the
  // frames already queued reach it. Behind it they never would.
  const int64_t last = m_last_dequeued_pts;
  if(last != AV_NOPTS_VALUE && flicks != 0 && target >= last
     && av_rescale_q(target - last, stream_tb, av_tb) <= (av_tb.den / 5) / av_tb.num)
    return false;

  // A stream without timestamps cannot be sought by time: it is restarted,
  // by bytes, and numbered from its beginning; the frames before the target
  // are skipped below, as DirectVideoNodeRenderer walks them.
  const bool no_timestamps = m_formatContext->iformat->flags & AVFMT_NOTIMESTAMPS;
  auto seek_to = [&](int64_t abs_flicks) {
    if(no_timestamps
       || !ossia::seek_to_flick(m_formatContext, m_codecContext, m_avstream, abs_flicks))
    {
      if(av_seek_frame(m_formatContext, m_avstream->index, 0, AVSEEK_FLAG_BYTE) < 0)
        return false;
      if(m_codecContext)
        avcodec_flush_buffers(m_codecContext);
      m_missingTimestamps.next = start_pts;
    }
    return true;
  };

  auto read_first = [&] {
    ReadFrame r;
    do
    {
      if(r.frame)
      {
        SCORE_LIBAV_FRAME_DEALLOC_CHECK(r.frame);
        av_frame_free(&r.frame);
      }

      auto pkt = av_packet_alloc();
      r = read_one_frame(*pkt);
      av_packet_unref(pkt);
      av_packet_free(&pkt);
    } while(r.error == AVERROR(EAGAIN));
    return r;
  };

  // Every frame enqueued from here on is after the seek; the reader drops
  // the ones before, held back or still queued.
  const int generation = m_frames.start_generation();
  const int64_t lead_flicks = av_rescale_q(m_ptsLead, stream_tb, flicks_time_base);
  if(!seek_to(absolute_flicks - lead_flicks))
  {
    qDebug() << "Failed to seek for time ";
    seek_generation.store(generation, std::memory_order_release);
    return false;
  }

  // The seek asks for the target minus the pts - dts lead measured so far.
  // Before any packet was read, a demuxer seeking on dts (MPEG-TS, MPEG-PS)
  // can still land on a frame that shows after the target: back off until
  // the first frame is not later. Frames queued by an attempt that landed
  // too late belong to this seek's generation but come before the frame
  // marked below, which the consumer discards up to.
  // A video starting after its container (audio first) legitimately shows
  // its first frame after any earlier target.
  const int64_t landing = m_avstream->start_time != AV_NOPTS_VALUE
                              ? std::max(target, m_avstream->start_time)
                              : target;
  ReadFrame r = read_first();
  int64_t back = 0;
  for(int attempt = 0; attempt < 6 && r.frame && r.frame->pts != AV_NOPTS_VALUE
                       && r.frame->pts > landing && absolute_flicks - back > 0;
      attempt++)
  {
    SCORE_LIBAV_FRAME_DEALLOC_CHECK(r.frame);
    av_frame_free(&r.frame);
    back = back ? back * 2
                : 2 * av_rescale_q(m_missingTimestamps.step, stream_tb, flicks_time_base);
    if(!seek_to(std::max<int64_t>(0, absolute_flicks - lead_flicks - back)))
      break;
    r = read_first();
  }
  while(no_timestamps && r.frame && r.frame->pts + m_missingTimestamps.step <= target)
  {
    SCORE_LIBAV_FRAME_DEALLOC_CHECK(r.frame);
    av_frame_free(&r.frame);
    r = read_first();
  }

  if(r.frame)
  {
    // Enqueue BEFORE publishing the discard marker. Otherwise the GFX thread's
    // discard_and_dequeue* can observe the marker for a frame not yet in
    // `available`, drain the queue and return r.frame as current while the
    // decoder still owns it and is about to enqueue it → double ownership /
    // UAF of the pixels a live zero-copy GPU upload references. With this
    // order the marker is only ever visible once its frame is already in the
    // queue, and the consumer falls through to a normal dequeue when the
    // marker frame isn't found.
    m_frames.enqueue(r.frame);
    m_frames.set_discard_frame(r.frame);
  }
  else
  {
    SCORE_LIBAV_FRAME_DEALLOC_CHECK(r.frame);
    av_frame_free(&r.frame);
  }

  m_finished = false;
  seek_generation.store(generation, std::memory_order_release);

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
    {
      const AVRational rate = m_avstream->avg_frame_rate.num > 0
                                  ? m_avstream->avg_frame_rate
                                  : m_avstream->r_frame_rate;
      start_pts = playbackStartPts(*m_formatContext, *m_avstream);
      time_base = tb;
      m_missingTimestamps = {};
      m_missingTimestamps.next = start_pts;
      if(rate.num > 0 && rate.den > 0)
        m_missingTimestamps.step
            = std::max<int64_t>(1, av_rescale_q(1, av_inv_q(rate), tb));
    }
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
        color_range = codecPar->color_range;
        color_primaries = codecPar->color_primaries;
        color_trc = codecPar->color_trc;
        color_space = codecPar->color_space;
        chroma_location = codecPar->chroma_location;

        // Detect wide-gamut / HDR evidence from primaries and transfer function.
        // This is used to infer color_space when it is unspecified.
        const bool has_bt2020_evidence =
            color_primaries == AVCOL_PRI_BT2020
            || color_trc == AVCOL_TRC_SMPTE2084      // PQ (HDR10 / BT.2100)
            || color_trc == AVCOL_TRC_ARIB_STD_B67;   // HLG (BT.2100)

        // Display P3 content may use BT.709 matrix coefficients
        // but with wider primaries. Don't force it to BT.2020.
        const bool has_p3_evidence =
            color_primaries == AVCOL_PRI_SMPTE432     // Display P3 (D65)
            || color_primaries == AVCOL_PRI_SMPTE431; // DCI-P3

        if(color_space == AVCOL_SPC_UNSPECIFIED)
        {
          if(has_bt2020_evidence)
            color_space = AVCOL_SPC_BT2020_NCL;
          else if(has_p3_evidence)
            // P3 content typically uses BT.709 matrix coefficients.
            // colorMatrix() will detect the P3 primaries and route
            // through the Display P3 pipeline.
            color_space = AVCOL_SPC_BT709;
          else if(codecPar->height < 625)
            color_space = AVCOL_SPC_SMPTE170M;
          else if(codecPar->height < 720)
            color_space = AVCOL_SPC_BT470BG;
          else
            color_space = AVCOL_SPC_BT709;
        }
        if(color_range == AVCOL_RANGE_UNSPECIFIED)
          color_range = AVCOL_RANGE_MPEG;

        // HDR handling
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
        {
          const auto data = codecPar->coded_side_data;
          const auto n = codecPar->nb_coded_side_data;
          // Light data
          if(auto sd = av_packet_side_data_get(data, n, AV_PKT_DATA_CONTENT_LIGHT_LEVEL))
            if(sd->data)
              this->content_light = *reinterpret_cast<const AVContentLightMetadata*>(sd->data);

          // Mastering side data
          if (auto sd = av_packet_side_data_get(data, n, AV_PKT_DATA_MASTERING_DISPLAY_METADATA))
            if(sd->data)
              this->mastering_display = *(AVMasteringDisplayMetadata *)sd->data;
        }
#endif

        codec_id = codecPar->codec_id;

        // Check if this is a GPU-direct codec (HAP or DXV DXT1/DXT5)
        bool use_gpu_direct = false;
        if(m_avstream->codecpar->codec_id == AV_CODEC_ID_HAP)
        {
          // HAP: store the FOURCC in the format for GPU decoder matching
          memcpy(&pixel_format, &m_avstream->codecpar->codec_tag, 4);
          use_gpu_direct = true;
        }
        else if(m_avstream->codecpar->codec_id == AV_CODEC_ID_DXV)
        {
          // DXV: peek first packet to determine sub-format (DXT1/DXT5)
          // Store synthetic fourcc in pixel_format for GPU decoder matching
          auto packet = av_packet_alloc();
          if(av_read_frame(m_formatContext, packet) >= 0 && packet->size >= 4)
          {
            uint32_t tag = packet->data[0] | (packet->data[1] << 8)
                           | (packet->data[2] << 16)
                           | ((uint32_t)packet->data[3] << 24);
            switch(tag)
            {
              case 0x44585431: // MKBETAG('D','X','T','1')
                memcpy(&pixel_format, "Dxv1", 4);
                use_gpu_direct = true;
                break;
              case 0x44585435: // MKBETAG('D','X','T','5')
                memcpy(&pixel_format, "Dxv5", 4);
                use_gpu_direct = true;
                break;
              case 0x59434736: // MKBETAG('Y','C','G','6')
                memcpy(&pixel_format, "DxvY", 4);
                use_gpu_direct = true;
                break;
              case 0x59473130: // MKBETAG('Y','G','1','0')
                memcpy(&pixel_format, "DxvA", 4);
                use_gpu_direct = true;
                break;
              default: {
                // Old format: check type flags in high byte
                uint8_t old_type = tag >> 24;
                if(old_type & 0x40)
                {
                  memcpy(&pixel_format, "Dxv5", 4);
                  use_gpu_direct = true;
                }
                else if(old_type & 0x20)
                {
                  memcpy(&pixel_format, "Dxv1", 4);
                  use_gpu_direct = true;
                }
                // Unknown old format falls through to avcodec
                break;
              }
            }
            av_packet_unref(packet);
          }
          av_packet_free(&packet);
          // Seek back to beginning regardless
          av_seek_frame(m_formatContext, m_avstream->index, 0, AVSEEK_FLAG_BACKWARD);
        }

        if(use_gpu_direct)
        {
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

          res = open_codec_context(*this, m_avstream, [this](AVCodecContext& ctx) {
            ctx.framerate
                = av_guess_frame_rate(m_formatContext, (AVStream*)m_avstream, NULL);
            m_codecContext->pkt_timebase = m_avstream->time_base;
            // m_codecContext->codec_id = m_codec->id;
          });

          if(m_codecContext)
          {
            auto tb = m_codecContext->pkt_timebase;
            dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
            flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;
          }
        }
      }
    }
  }

  if(res && m_avstream)
  {
    const auto probe
        = classifyFrameAccess(*m_formatContext, *m_avstream, m_inputFile.c_str());
    frame_access = probe.access;
    max_keyframe_gap = probe.max_gap;
  }
  else
  {
    frame_access = FrameAccess::Unknown;
    max_keyframe_gap = -1;
    close_video();
  }
  return res;
}

void VideoDecoder::close_video() noexcept
{
  if(m_codecContext)
  {
    avcodec_flush_buffers(m_codecContext);
#if defined(__APPLE__)
#if FF_API_VT_HWACCEL_CONTEXT
    if(m_codecContext->hwaccel_context)
      av_videotoolbox_default_free(m_codecContext);
#endif
#endif
    avcodec_free_context(&m_codecContext);

    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  m_rescale.close();

  m_avstream = nullptr;
}
}
#endif

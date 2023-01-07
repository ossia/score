#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/FrameQueue.hpp>
#include <Video/VideoInterface.hpp>

extern "C" {
struct SwsContext;
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <QDebug>

#include <score_plugin_media_export.h>
namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT Rescale
{
public:
  operator bool() const noexcept { return m_rescale; }

  void open(const VideoMetadata& src);
  void close();
  void rescale(FrameQueue& m_frames, AVFramePointer& frame, ReadFrame& read);

private:
  const VideoMetadata* m_src{};
  SwsContext* m_rescale{};
};

struct SCORE_PLUGIN_MEDIA_EXPORT DecoderConfiguration
{
  AVPixelFormat hardwareAcceleration{AV_PIX_FMT_NONE};
  std::string decoder;
  int threads{};
  bool useAVCodec{true};
};

struct SCORE_PLUGIN_MEDIA_EXPORT LibAVDecoder
{
  ReadFrame enqueue_frame(const AVPacket* pkt) noexcept;
  std::pair<AVBufferRef*, const AVCodec*> open_hwdec(const AVCodec&) noexcept;

  bool open_codec_context(
      VideoInterface& self, const AVStream* stream,
      std::function<void(AVCodecContext&)> setup);
  void init_scaler(VideoInterface& self) noexcept;
  void load_packet_in_frame(const AVPacket& packet, AVFrame& frame);

  ReadFrame read_one_frame(AVPacket& packet);
  ReadFrame read_one_frame_raw(AVPacket& packet);
  ReadFrame read_one_frame_avcodec(AVPacket& packet);

  DecoderConfiguration m_conf;

  AVFormatContext* m_formatContext{};
  AVStream* m_avstream{};
  const AVCodec* m_codec{};
  AVCodecContext* m_codecContext{};

  FrameQueue m_frames;
  Rescale m_rescale;
  bool m_finished{};
};

}
#endif

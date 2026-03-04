#pragma once
#include <Media/Libav.hpp>
#include <Video/VideoEnums.hpp>
#if SCORE_HAS_LIBAV
#include <score_plugin_media_export.h>
extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
#include <libavcodec/version.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
#if __has_include(<libavutil/mastering_display_metadata.h>)
#include <libavutil/mastering_display_metadata.h>
#endif
#endif
struct AVFrame;
struct AVCodecContext;
struct AVPacket;
}
#include <memory>
#include <string>

namespace Video
{

struct SCORE_PLUGIN_MEDIA_EXPORT ImageFormat
{
  // From ffmpeg
  int width{};
  int height{};
  AVPixelFormat pixel_format = AVPixelFormat(-1);

  AVColorRange color_range = AVColorRange(-1);
  AVColorPrimaries color_primaries = AVColorPrimaries(-1);
  AVColorTransferCharacteristic color_trc = AVColorTransferCharacteristic(-1);
  AVColorSpace color_space = AVColorSpace(-1);
  AVChromaLocation chroma_location = AVChromaLocation(-1);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
  AVMasteringDisplayMetadata mastering_display{};
  std::optional<AVContentLightMetadata> content_light{};
#endif

  // Set by the user
  OutputFormat output_format = ::Video::OutputFormat::SDR;
  Tonemap tonemap = ::Video::Tonemap::BT_2390;
};

struct SCORE_PLUGIN_MEDIA_EXPORT VideoMetadata : ImageFormat
{
  std::string filePath;
  AVCodecID codec_id = AV_CODEC_ID_NONE;
  double fps{};
  bool realTime{};
  double flicks_per_dts{};
  double dts_per_flicks{};
};

struct SCORE_PLUGIN_MEDIA_EXPORT VideoInterface : VideoMetadata
{
  virtual ~VideoInterface();
  virtual AVFrame* dequeue_frame() noexcept = 0;
  virtual void release_frame(AVFrame* frame) noexcept = 0;
};

struct SCORE_PLUGIN_MEDIA_EXPORT ReadFrame
{
  AVFrame* frame{};
  int error{};
};
struct SCORE_PLUGIN_MEDIA_EXPORT FreeAVFrame
{
  void operator()(AVFrame* f) const noexcept;
};

using AVFramePointer = std::unique_ptr<AVFrame, FreeAVFrame>;

ReadFrame readVideoFrame(
    AVCodecContext* codecContext, const AVPacket* pkt, AVFrame* frame, bool ignorePts);
}
#endif

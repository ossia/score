#pragma once
#include <Media/Libav.hpp>
#include <Video/VideoEnums.hpp>
#include <Video/VideoPixelFormat.hpp>
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

  // For hardware-accelerated formats (AV_PIX_FMT_DRM_PRIME, VAAPI,
  // CUDA, etc.) where `pixel_format` is an opaque HW tag, this carries
  // the underlying SW pixel format that the descriptor / surface wraps
  AVPixelFormat hwaccel_sw_format = AVPixelFormat(-1);

  /** For a wire layout no AVPixelFormat describes, the format that does.
   *  NDI's UYVA and PA16 are the cases: both carry a separate alpha plane
   *  after a packed or semi-planar picture, and ffmpeg has no format for
   *  either shape. When this is set it decides the decoder; `pixel_format`
   *  still carries the closest AVPixelFormat for everything that reads one. */
  VideoPixelFormat native_format = VideoPixelFormat::Unknown;

  // Unset means FFmpeg's own UNSPECIFIED, not -1: none of these enums has a
  // negative enumerator, so -1 is outside their range and reading it is UB.
  // Every consumer already treats UNSPECIFIED like an unknown value.
  AVColorRange color_range = AVCOL_RANGE_UNSPECIFIED;
  AVColorPrimaries color_primaries = AVCOL_PRI_UNSPECIFIED;
  AVColorTransferCharacteristic color_trc = AVCOL_TRC_UNSPECIFIED;
  AVColorSpace color_space = AVCOL_SPC_UNSPECIFIED;
  AVChromaLocation chroma_location = AVCHROMA_LOC_UNSPECIFIED;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
  AVMasteringDisplayMetadata mastering_display{};
  std::optional<AVContentLightMetadata> content_light{};
#endif

  /** How the source delivers fields, if it does. Set by whoever produces the
   *  frames; `Fields` means `height` is the PICTURE height while each AVFrame
   *  is half of it, which VideoNodeRenderer::needsRebuild knows about. */
  Interlacing interlacing = ::Video::Interlacing::None;

  // Set by the user
  OutputFormat output_format = ::Video::OutputFormat::SDR;
  Tonemap tonemap = ::Video::Tonemap::Auto;
  Deinterlace deinterlace = ::Video::Deinterlace::Weave;
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

ReadFrame receiveVideoFrame(
    AVCodecContext* codecContext, AVFrame* frame, bool ignorePts);
}
#endif

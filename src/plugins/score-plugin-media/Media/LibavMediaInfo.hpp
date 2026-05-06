#pragma once
#include <Media/Libav.hpp>

#include <score_plugin_media_export.h>

#if SCORE_HAS_LIBAV

#include <QString>

#include <optional>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::libav
{

/**
 * @brief Result of probing a media file.
 *
 * `duration_av` is in AV_TIME_BASE units (microseconds). Use
 * `duration_seconds()` for a convenient floating-point conversion.
 *
 * `streams` lists the media type of every stream, in order.
 *
 * `format_context` is the opened, fully-analyzed AVFormatContext. The caller
 * may use it to inspect codec parameters, metadata, etc. It is closed when
 * this object is destroyed.
 */
struct SCORE_PLUGIN_MEDIA_EXPORT MediaInfo
{
  struct FormatContextDeleter
  {
    void operator()(AVFormatContext* ctx) const noexcept
    {
      if(ctx)
        avformat_close_input(&ctx);
    }
  };
  using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

  FormatContextPtr format_context;
  std::vector<AVMediaType> streams;
  std::optional<int64_t> duration_av; // in AV_TIME_BASE units; empty == unknown

  std::optional<double> duration_seconds() const noexcept
  {
    if(!duration_av)
      return std::nullopt;
    return *duration_av / double(AV_TIME_BASE);
  }

  bool has_video() const noexcept
  {
    for(auto t : streams)
      if(t == AVMEDIA_TYPE_VIDEO)
        return true;
    return false;
  }

  bool has_audio() const noexcept
  {
    for(auto t : streams)
      if(t == AVMEDIA_TYPE_AUDIO)
        return true;
    return false;
  }
};

/**
 * @brief Probe a media file's structure and duration, robust to broken files.
 *
 * Tries progressively more aggressive strategies:
 *   1. Default open — fast path for well-formed files.
 *   2. Large probesize + analyzeduration — for files with sparse headers.
 *   3. +ignidx +genpts flags — for AVIs / containers with corrupt indices.
 *   4. Full packet scan — last-resort manual duration computation.
 *
 * Returns std::nullopt only if the file cannot be opened at all. If the file
 * opens but no duration can be computed, returns a MediaInfo with
 * duration_av == 0 (caller can decide whether that's acceptable).
 */
SCORE_PLUGIN_MEDIA_EXPORT
std::optional<MediaInfo> probe(const QString& path);

/**
 * @brief Same as probe(), but throws std::runtime_error on failure.
 *
 * Convenience for code paths (e.g. AudioDecoder::read_length) that previously
 * threw on open failure.
 */
SCORE_PLUGIN_MEDIA_EXPORT
MediaInfo probe_or_throw(const QString& path);

} // namespace score::libav

#endif
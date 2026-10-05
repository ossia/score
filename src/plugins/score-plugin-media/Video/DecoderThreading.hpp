#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/cpu.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

/**
 * @file DecoderThreading.hpp
 * @brief Which of libavcodec's threading models a decoder runs, and how wide.
 *
 * Frame threading works for any stream but delays output by one frame per
 * thread and keeps a frame per thread alive. Slice threading adds no delay but
 * only scales with the slices / tiles / rows of the bitstream (single-tile
 * JPEG 2000: 6x slower than frame threads; ProRes: 1.7x faster).
 *
 * libavcodec does not report how many slices a stream carries, so the choice
 * uses the decoder's capabilities, the DecodeUseCase, and a list of formats
 * whose bitstream mandates many slices per frame.
 */
namespace Video
{

//! What consumes the decoded frames, which sets how much delay is acceptable.
enum class DecodeUseCase : uint8_t
{
  //! Frames are queued ahead of display: throughput first, the queue absorbs
  //! the frame-threading delay.
  Playback,
  //! A packet in must give its frame out (per-frame seeking, scrubbing).
  FrameExact,
  //! Cameras and network streams: any delay is visible latency.
  Live,
  //! One frame is decoded, then the context is dropped.
  Thumbnail
};

struct DecoderThreading
{
  int count{1};      //!< AVCodecContext::thread_count; 0 lets the codec pick
  int type{};        //!< FF_THREAD_FRAME / FF_THREAD_SLICE, 0 leaves the default
  bool zeroDelay{};  //!< ask a codec with its own thread pool for no frame delay
  const char* reason{""};
};

namespace threading
{
//! ffmpeg's own cap on automatic thread counts. Frame threads beyond it were
//! measured slower, not faster, on 4K JPEG 2000.
inline constexpr int max_frame_threads = 16;
//! Slice threads do keep scaling past 16 on slice-rich formats (ProRes 4K:
//! +30% from 16 to 32 threads).
inline constexpr int max_slice_threads = 64;
//! Upper bound for the decoded frames frame threading keeps in flight.
inline constexpr int64_t frame_memory_budget = int64_t(1) << 30;

//! Formats whose bitstream mandates many independent units per frame, so that
//! slice threads match or beat frame threads at no delay. Keyed by decoder
//! name, which is stable across libavcodec versions where some codec ids are
//! not.
inline bool sliceRich(const AVCodec& codec) noexcept
{
  static constexpr std::string_view names[]{
      "prores",   // slices of at most 8 macroblocks
      "dnxhd",    // one slice per macroblock row
      "dvvideo",  // fixed DIF segments
      "hap",      // texture blocks, decompressed in row chunks
      "dxv",      // same
      "exr",      // scanline blocks or tiles
      "v210",     // row-independent unpacking
  };
  const std::string_view name = codec.name ? codec.name : "";
  return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

//! Wrappers whose own thread pool splits a frame (tiles, rows) rather than
//! pipelining frames, and therefore never delays output.
inline bool ownPoolHasNoFrameDelay(const AVCodec& codec) noexcept
{
  static constexpr std::string_view names[]{
      "libvpx", "libvpx-vp9", "libaom-av1", "libjxl", "libjxl_anim"};
  const std::string_view name = codec.name ? codec.name : "";
  return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

//! Wrappers whose pipelining can be switched off (libdav1d).
inline bool ownPoolHasDelayOption(const AVCodec& codec) noexcept
{
  return codec.priv_class
         && av_opt_find(
             (void*)&codec.priv_class, "max_frame_delay", nullptr, 0,
             AV_OPT_SEARCH_FAKE_OBJ);
}

inline int cpuCount() noexcept
{
  return std::max(1, av_cpu_count());
}

inline int64_t frameBytes(const AVCodecParameters* par) noexcept
{
  if(!par || par->width <= 0 || par->height <= 0)
    return 0;
  if(par->format >= 0)
  {
    const int sz = av_image_get_buffer_size(
        (AVPixelFormat)par->format, par->width, par->height, 1);
    if(sz > 0)
      return sz;
  }
  return int64_t(par->width) * par->height * 4;
}

inline int frameThreads(const AVCodecParameters* par) noexcept
{
  int n = std::min(cpuCount(), max_frame_threads);
  if(const auto bytes = frameBytes(par); bytes > 0)
    n = std::min<int64_t>(n, std::max<int64_t>(2, frame_memory_budget / bytes));
  return n;
}

inline int sliceThreads(const AVCodecParameters* par) noexcept
{
  int n = std::min(cpuCount(), max_slice_threads);
  // No codec slices finer than a 16-line macroblock row.
  if(par && par->height > 0)
    n = std::min(n, std::max(1, (par->height + 15) / 16));
  return n;
}

//! SCORE_VIDEO_THREADING=frame|slice|none|auto forces the model,
//! SCORE_VIDEO_THREADS=N the count; for diagnosing a misbehaving codec.
inline int envThreadType(bool& set) noexcept
{
  set = false;
  const char* env = std::getenv("SCORE_VIDEO_THREADING");
  if(!env)
    return 0;
  const std::string_view v = env;
  set = true;
  if(v == "frame")
    return FF_THREAD_FRAME;
  if(v == "slice")
    return FF_THREAD_SLICE;
  if(v == "none")
    return 0;
  set = false;
  return 0;
}

inline int envThreadCount() noexcept
{
  const char* env = std::getenv("SCORE_VIDEO_THREADS");
  return env ? std::max(0, std::atoi(env)) : 0;
}
}

/**
 * @param userThreads a thread count the user asked for, 0 for automatic. It
 *        sets the width only: the model stays the policy's, since forcing
 *        slices on a single-slice stream would single-thread it.
 * @param hardware the context decodes through a hardware device.
 */
inline DecoderThreading chooseDecoderThreading(
    const AVCodec& codec, const AVCodecParameters* par, DecodeUseCase use,
    int userThreads = 0, bool hardware = false) noexcept
{
  using namespace threading;
#if defined(__EMSCRIPTEN__)
  // A multithreaded context crashes on teardown on the emscripten pthread
  // runtime, even when no frame was decoded.
  return {1, 0, false, "single-threaded on wasm"};
#endif
  if(hardware || (codec.capabilities & AV_CODEC_CAP_HARDWARE))
    return {1, 0, false, "hardware decoder"};

  if(const int n = envThreadCount(); n > 0)
    userThreads = n;
  bool forced{};
  const int forcedType = envThreadType(forced);

  const int caps = codec.capabilities;
  const bool canFrame = caps & AV_CODEC_CAP_FRAME_THREADS;
  const bool canSlice = caps & AV_CODEC_CAP_SLICE_THREADS;
  const bool delayOk = use == DecodeUseCase::Playback;

  if(caps & AV_CODEC_CAP_OTHER_THREADS)
  {
    // Some wrappers (older libdav1d) map FF_THREAD_FRAME onto their frame
    // pipelining, so the mask is what keeps it on or off.
    const int count = userThreads > 0 ? userThreads : 0;
    if(delayOk)
      return {
          count, FF_THREAD_FRAME | FF_THREAD_SLICE, false, "codec's own thread pool"};
    if(ownPoolHasNoFrameDelay(codec))
      return {count, FF_THREAD_SLICE, false, "codec's own thread pool, no frame delay"};
    if(ownPoolHasDelayOption(codec))
      return {
          count, FF_THREAD_SLICE, true, "codec's own thread pool, frame delay disabled"};
    return {1, 0, false, "codec's own thread pool may delay frames"};
  }

  if(forced)
  {
    if(forcedType == FF_THREAD_FRAME && canFrame)
      return {
          userThreads > 0 ? userThreads : frameThreads(par), FF_THREAD_FRAME,
          false, "forced by SCORE_VIDEO_THREADING"};
    if(forcedType == FF_THREAD_SLICE && canSlice)
      return {
          userThreads > 0 ? userThreads : sliceThreads(par), FF_THREAD_SLICE,
          false, "forced by SCORE_VIDEO_THREADING"};
    if(forcedType == 0)
      return {1, 0, false, "forced by SCORE_VIDEO_THREADING"};
  }

  if(delayOk && canFrame && !(canSlice && sliceRich(codec)))
    return {
        userThreads > 0 ? userThreads : frameThreads(par), FF_THREAD_FRAME, false,
        canSlice ? "frame threads: the stream may hold a single slice"
                 : "frame threads: the only model the codec has"};

  if(canSlice)
    return {
        userThreads > 0 ? userThreads : sliceThreads(par), FF_THREAD_SLICE, false,
        !delayOk     ? "slice threads: no frame delay allowed"
        : canFrame   ? "slice threads: the format mandates many slices"
                     : "slice threads: the only model the codec has"};

  if(canFrame)
    return {1, 0, false, "frame threading would delay output"};
  return {1, 0, false, "codec has no threading"};
}

//! Applies to a context allocated for its codec, before avcodec_open2.
inline void applyDecoderThreading(AVCodecContext& ctx, const DecoderThreading& t) noexcept
{
  ctx.thread_count = t.count;
  if(t.type)
    ctx.thread_type = t.type;
  if(t.zeroDelay && ctx.priv_data)
    av_opt_set_int(ctx.priv_data, "max_frame_delay", 1, 0);
}

//! One line for the log, from the context once avcodec_open2 succeeded:
//! libavcodec may have narrowed the request.
inline std::string describeDecoderThreading(
    const AVCodecContext& ctx, const DecoderThreading& t)
{
  std::string s = ctx.codec && ctx.codec->name ? ctx.codec->name : "?";
  switch(ctx.active_thread_type)
  {
    case FF_THREAD_FRAME:
      s += ": frame threads x";
      break;
    case FF_THREAD_SLICE:
      s += ": slice threads x";
      break;
    default:
      if(ctx.codec && (ctx.codec->capabilities & AV_CODEC_CAP_OTHER_THREADS))
        s += ctx.thread_count > 0 ? ": own thread pool x" : ": own thread pool, auto";
      else
        s += ": single thread";
      break;
  }
  if(ctx.active_thread_type
     || (ctx.thread_count > 0 && ctx.codec
         && (ctx.codec->capabilities & AV_CODEC_CAP_OTHER_THREADS)))
    s += std::to_string(ctx.thread_count);
  s += " (";
  s += t.reason;
  s += ")";
  return s;
}

}
#endif

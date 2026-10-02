// Video::chooseDecoderThreading(): which libavcodec threading model each
// decoder runs with, for each kind of consumer.
//
// The decisions are pure functions of the AVCodec, its stream parameters and
// the use case, so they are checked against the real decoders of the linked
// libavcodec. A decoder missing from the build is skipped, not failed.

#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV

#include <Video/DecoderThreading.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

using Video::DecodeUseCase;

namespace
{
struct Stream
{
  AVCodecParameters* par = avcodec_parameters_alloc();
  Stream(int w, int h, AVPixelFormat fmt)
  {
    par->width = w;
    par->height = h;
    par->format = fmt;
  }
  ~Stream() { avcodec_parameters_free(&par); }
};

Video::DecoderThreading
choose(const AVCodec& c, DecodeUseCase use, int user = 0, bool hw = false)
{
  Stream s{3840, 2160, AV_PIX_FMT_YUV422P10LE};
  return Video::chooseDecoderThreading(c, s.par, use, user, hw);
}

const AVCodec* decoder(const char* name)
{
  return avcodec_find_decoder_by_name(name);
}

void setEnv(const char* name, const char* value)
{
#if defined(_WIN32)
  _putenv_s(name, value ? value : "");
#else
  if(value)
    setenv(name, value, 1);
  else
    unsetenv(name);
#endif
}

struct NoEnvOverride
{
  // The policy honours these for diagnosis; a developer's shell must not
  // change what the suite asserts.
  NoEnvOverride()
  {
    setEnv("SCORE_VIDEO_THREADING", nullptr);
    setEnv("SCORE_VIDEO_THREADS", nullptr);
  }
  ~NoEnvOverride() { setEnv("SCORE_VIDEO_THREADING", nullptr); }
};
}

TEST_CASE("Playback favours frame threads unless the format is slice-rich",
          "[video][threading]")
{
  NoEnvOverride env;
  // Single-tile DCPs and single-slice H.264: slice threads would not scale.
  for(auto name : {"jpeg2000", "h264", "hevc", "vp9"})
  {
    if(auto c = decoder(name))
    {
      INFO(name);
      CHECK(choose(*c, DecodeUseCase::Playback).type == FF_THREAD_FRAME);
    }
  }
  // Hundreds of independent slices per frame: as fast at no delay.
  for(auto name : {"prores", "dnxhd"})
  {
    if(auto c = decoder(name))
    {
      INFO(name);
      CHECK(choose(*c, DecodeUseCase::Playback).type == FF_THREAD_SLICE);
    }
  }
  // Only one model: it is the one used.
  if(auto c = decoder("mpeg2video"))
    CHECK(choose(*c, DecodeUseCase::Playback).type == FF_THREAD_SLICE);
  if(auto c = decoder("png"))
    CHECK(choose(*c, DecodeUseCase::Playback).type == FF_THREAD_FRAME);
}

TEST_CASE("No frame threads where delay is not allowed", "[video][threading]")
{
  NoEnvOverride env;
  for(auto use : {DecodeUseCase::FrameExact, DecodeUseCase::Live, DecodeUseCase::Thumbnail})
  {
    if(auto c = decoder("jpeg2000"))
    {
      const auto t = choose(*c, use);
      CHECK(t.type == FF_THREAD_SLICE);
      CHECK(t.count > 1);
    }
    // Frame threads are all a PNG decoder has: it runs on one thread.
    if(auto c = decoder("png"))
    {
      const auto t = choose(*c, use);
      CHECK(t.type == 0);
      CHECK(t.count == 1);
    }
  }
}

TEST_CASE("A user thread count sizes the pool without changing the model",
          "[video][threading]")
{
  NoEnvOverride env;
  if(auto c = decoder("jpeg2000"))
  {
    // Slice threads on a single-tile stream would leave all but one idle.
    const auto t = choose(*c, DecodeUseCase::Playback, 4);
    CHECK(t.type == FF_THREAD_FRAME);
    CHECK(t.count == 4);
  }
}

TEST_CASE("Hardware decoding stays single-threaded", "[video][threading]")
{
  NoEnvOverride env;
  if(auto c = decoder("h264"))
  {
    const auto t = choose(*c, DecodeUseCase::Playback, 8, /*hw=*/true);
    CHECK(t.count == 1);
    CHECK(t.type == 0);
  }
}

TEST_CASE("Codecs with their own thread pool", "[video][threading]")
{
  NoEnvOverride env;
  if(auto c = decoder("libdav1d"))
  {
    const auto play = choose(*c, DecodeUseCase::Playback);
    CHECK(play.count == 0);
    CHECK_FALSE(play.zeroDelay);

    // Its frame pipelining can be switched off, which keeps tile threads.
    const auto exact = choose(*c, DecodeUseCase::FrameExact);
    CHECK(exact.count == 0);
    CHECK(exact.zeroDelay);
  }
  if(auto c = decoder("libvpx-vp9"))
  {
    // Tile threads only: no delay to remove.
    const auto exact = choose(*c, DecodeUseCase::FrameExact);
    CHECK(exact.count == 0);
    CHECK_FALSE(exact.zeroDelay);
  }
}

TEST_CASE("Frame threads are bounded by the frames they keep alive",
          "[video][threading]")
{
  NoEnvOverride env;
  if(auto c = decoder("prores"))
  {
    // 16K RGBA 16-bit: ~1 GiB per frame, so only the floor of two remains.
    Stream s{15360, 8640, AV_PIX_FMT_RGBA64LE};
    setEnv("SCORE_VIDEO_THREADING", "frame");
    const auto t = Video::chooseDecoderThreading(*c, s.par, DecodeUseCase::Playback);
    CHECK(t.type == FF_THREAD_FRAME);
    CHECK(t.count <= 2);
  }
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("decoder threading tests need libav", "[video][threading]") { }

#endif

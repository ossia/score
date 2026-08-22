#pragma once

// The known-pixel master clip provisioned by tests/hardware/with-virtual-media.sh
// --matrix, and the comparison every harness that asserts a decoded picture uses.
//
// The master is a grid of solid blocks whose colour is a function of the block
// index and the frame number, rotating by one column per frame. master.rgb is
// derived FROM master.nut by the runner rather than generated a second time, so
// the ground truth loaded here and the bytes the encoders were fed cannot drift
// apart. Geometry is recovered twice and independently -- from the container by
// libav, and from the length of the raw dump -- and the two must agree.
//
// Requires libavformat, libavutil and libswscale on the including target.

#include <Video/Rescale.hpp>

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace score::test::video
{

// Pixels within this many of a block edge are dropped from every comparison:
// that is where 4:2:0 chroma and a deblocking filter legitimately blend two
// colours, and it is the only part of the picture a codec is allowed to move.
inline constexpr int kBlockMargin = 10;

// The fidelity a row is entitled to: `exact` is an RGB lossless codec and must
// come back byte for byte; `yuvexact` is lossless but round trips through a YUV
// colour space, so it carries the rounding of one conversion; `lossy` carries a
// quantiser. Harnesses assert separately that two DIFFERENT master frames are
// further apart than twice the largest of these.
inline constexpr int kToleranceExact = 0;
inline constexpr int kToleranceYuvExact = 6;
inline constexpr int kToleranceLossy = 64;
inline constexpr int kLargestTolerance = kToleranceLossy;

inline QString matrixDir()
{
  const auto d = QString::fromLocal8Bit(qgetenv("SCORE_TEST_MATRIX_DIR"));
  REQUIRE_FALSE(d.isEmpty());
  REQUIRE(QFileInfo(d).isDir());
  return d;
}

inline std::string matrixPath(const char* name)
{
  const QString p = matrixDir() + QLatin1String("/") + QLatin1String(name);
  return p.toStdString();
}

// No hardware accelerator and one thread: the decode path under test must be
// the same on every host, including the ones whose GPU changed.
inline Video::DecoderConfiguration softwareOnly()
{
  Video::DecoderConfiguration conf;
  conf.hardwareAcceleration = AV_PIX_FMT_NONE;
  conf.threads = 1;
  conf.useAVCodec = true;
  return conf;
}

inline int blockSize()
{
  const int b = qgetenv("SCORE_TEST_MATRIX_BLOCK").toInt();
  return b > 0 ? b : 40;
}

inline int oddBlockSize()
{
  const int b = qgetenv("SCORE_TEST_MATRIX_ODD_BLOCK").toInt();
  return b > 0 ? b : 13;
}

struct Master
{
  int width{}, height{}, frames{};
  std::vector<uint8_t> rgb;

  const uint8_t* frame(int i) const
  {
    return rgb.data() + std::size_t(i) * std::size_t(width) * height * 3;
  }
};

// The master pair provisioned by the runner: `stem`.nut carries the picture,
// `stem`.rgb is the same frames flattened to rgb24 by the runner itself. Two
// stems exist: "master" (320x240, 40-pixel blocks) and "master-odd" (65x39,
// 13-pixel blocks), the second so that every odd-dimension chroma rounding is
// exercised by a picture and not only by a stride computation.
inline Master loadMaster(const char* stem = "master")
{
  Master m;

  AVFormatContext* ctx{};
  const auto nut = matrixPath((std::string{stem} + ".nut").c_str());
  REQUIRE(avformat_open_input(&ctx, nut.c_str(), nullptr, nullptr) == 0);
  REQUIRE(avformat_find_stream_info(ctx, nullptr) >= 0);
  for(unsigned i = 0; i < ctx->nb_streams; i++)
  {
    auto* par = ctx->streams[i]->codecpar;
    if(par->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      m.width = par->width;
      m.height = par->height;
      break;
    }
  }
  avformat_close_input(&ctx);
  REQUIRE(m.width > 0);
  REQUIRE(m.height > 0);

  const auto raw = matrixPath((std::string{stem} + ".rgb").c_str());
  FILE* f = std::fopen(raw.c_str(), "rb");
  REQUIRE(f != nullptr);
  std::fseek(f, 0, SEEK_END);
  const long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  REQUIRE(sz > 0);

  const long perFrame = long(m.width) * m.height * 3;
  REQUIRE(sz % perFrame == 0);
  m.frames = int(sz / perFrame);
  REQUIRE(m.frames >= 4);

  m.rgb.resize(std::size_t(sz));
  REQUIRE(std::fread(m.rgb.data(), 1, std::size_t(sz), f) == std::size_t(sz));
  std::fclose(f);

  // The env the runner exported is a cross-check on the two independent reads
  // above, never the source of either. It describes the default master only.
  if(std::string{stem} != "master")
    return m;

  const int envW = qgetenv("SCORE_TEST_MATRIX_WIDTH").toInt();
  const int envH = qgetenv("SCORE_TEST_MATRIX_HEIGHT").toInt();
  const int envN = qgetenv("SCORE_TEST_MATRIX_FRAMES").toInt();
  if(envW > 0)
    CHECK(m.width == envW);
  if(envH > 0)
    CHECK(m.height == envH);
  if(envN > 0)
    CHECK(m.frames == envN);

  return m;
}

// One decoded AVFrame as rgb24, using swscale on the test side so that every
// codec's native pixel format is compared in one space.
inline std::vector<uint8_t> toRgb24(const AVFrame& f, int w, int h)
{
  if(f.width <= 0 || f.height <= 0)
    return {};
  std::vector<uint8_t> out(std::size_t(w) * h * 3);
  SwsContext* sws = sws_getContext(
      f.width, f.height, AVPixelFormat(f.format), w, h, AV_PIX_FMT_RGB24,
      SWS_BILINEAR, nullptr, nullptr, nullptr);
  if(!sws)
    return {};
  uint8_t* dst[4]{out.data(), nullptr, nullptr, nullptr};
  int stride[4]{w * 3, 0, 0, 0};
  sws_scale(sws, f.data, f.linesize, 0, f.height, dst, stride);
  sws_freeContext(sws);
  return out;
}

// Per-pixel distance over the interior of every block.
struct Diff
{
  int maxDev{};
  double meanDev{};
  long compared{};
};

inline Diff blockDiff(
    const uint8_t* got, const uint8_t* want, int w, int h, int block,
    int margin = kBlockMargin)
{
  Diff d;
  long sum = 0;
  for(int by = 0; by + block <= h; by += block)
  {
    for(int bx = 0; bx + block <= w; bx += block)
    {
      for(int y = by + margin; y < by + block - margin; y++)
      {
        for(int x = bx + margin; x < bx + block - margin; x++)
        {
          const std::size_t o = (std::size_t(y) * w + x) * 3;
          for(int c = 0; c < 3; c++)
          {
            const int dev = std::abs(int(got[o + c]) - int(want[o + c]));
            if(dev > d.maxDev)
              d.maxDev = dev;
            sum += dev;
            d.compared++;
          }
        }
      }
    }
  }
  if(d.compared > 0)
    d.meanDev = double(sum) / double(d.compared);
  return d;
}

// The master frame a decoded picture is closest to, by mean deviation, and how
// far it is. -1 when the picture could not be converted.
struct BestMatch
{
  int index{-1};
  int maxDev{};
  double meanDev{};
};

inline BestMatch
bestMatch(const AVFrame& f, const Master& m, int block, int margin = kBlockMargin)
{
  BestMatch best;
  const auto rgb = toRgb24(f, m.width, m.height);
  if(rgb.size() != std::size_t(m.width) * m.height * 3)
    return best;

  double bestMean = 1e30;
  for(int j = 0; j < m.frames; j++)
  {
    const auto d = blockDiff(rgb.data(), m.frame(j), m.width, m.height, block, margin);
    if(d.compared > 0 && d.meanDev < bestMean)
    {
      bestMean = d.meanDev;
      best.index = j;
      best.maxDev = d.maxDev;
      best.meanDev = d.meanDev;
    }
  }
  return best;
}

}

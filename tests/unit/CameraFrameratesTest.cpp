// Gfx/CameraDevice: the framerate reduction applied to every camera
// enumeration (v4l2, DirectShow, AVFoundation).
//
// A camera announces one mode per (format, resolution, framerate) triple, which
// fills the device list with dozens of near-identical entries. Only the fastest
// mode of each resolution is kept, unless SCORE_GFX_CAMERA_ALL_FRAMERATES is
// set -- the env-var half runs as its own executable because the flag is read
// once per process.
//
// The invariant the whole reduction rests on: findBestCameraMode picks the same
// mode whether or not the list went through it.

#include <Gfx/CameraDevice.hpp>

#include <QtGlobal>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

#include <utility>
#include <vector>

using Gfx::CameraSettings;
using mode_list = std::vector<std::pair<CameraSettings, QString>>;

namespace
{
CameraSettings mode(
    QSize size, double fps, int codec = AV_CODEC_ID_RAWVIDEO,
    int pixelformat = AV_PIX_FMT_YUYV422)
{
  CameraSettings s;
  s.input = "v4l2";
  s.device = "/dev/video0";
  s.size = size;
  s.fps = fps;
  s.codec = codec;
  s.pixelformat = pixelformat;
  return s;
}

mode_list with_descriptions(std::vector<CameraSettings> modes)
{
  mode_list out;
  for(auto& m : modes)
    out.emplace_back(
        m, QString("%1x%2@%3").arg(m.size.width()).arg(m.size.height()).arg(m.fps));
  return out;
}

std::vector<CameraSettings> settings_of(const mode_list& modes)
{
  std::vector<CameraSettings> out;
  for(auto& [set, desc] : modes)
    out.push_back(set);
  return out;
}
}

#if defined(SCORE_TEST_CAMERA_ALL_FRAMERATES)
namespace
{
const bool force_all = [] {
  qputenv("SCORE_GFX_CAMERA_ALL_FRAMERATES", "1");
  return true;
}();
}

TEST_CASE("SCORE_GFX_CAMERA_ALL_FRAMERATES keeps every framerate", "[camera]")
{
  REQUIRE(force_all);
  REQUIRE(Gfx::cameraShowAllFramerates());

  auto modes = with_descriptions(
      {mode({1920, 1080}, 30.), mode({1920, 1080}, 15.), mode({1920, 1080}, 5.),
       mode({640, 480}, 30.), mode({640, 480}, 30.)});
  const auto before = modes;

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == before.size());
  for(std::size_t i = 0; i < modes.size(); i++)
  {
    CHECK(modes[i].first.size == before[i].first.size);
    CHECK(modes[i].first.fps == before[i].first.fps);
    CHECK(modes[i].second == before[i].second);
  }
}

#else

TEST_CASE("keepHighestFramerates: one entry per resolution, fastest kept", "[camera]")
{
  REQUIRE_FALSE(Gfx::cameraShowAllFramerates());

  auto modes = with_descriptions(
      {mode({1920, 1080}, 5.), mode({1920, 1080}, 30.), mode({1920, 1080}, 15.),
       mode({640, 480}, 30.), mode({640, 480}, 60.)});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 2);
  CHECK(modes[0].first.size == QSize{1920, 1080});
  CHECK(modes[0].first.fps == 30.);
  CHECK(modes[1].first.size == QSize{640, 480});
  CHECK(modes[1].first.fps == 60.);
}

TEST_CASE("keepHighestFramerates: the kept description is the kept mode's", "[camera]")
{
  auto modes = with_descriptions({mode({1280, 720}, 10.), mode({1280, 720}, 60.)});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 1);
  CHECK(modes[0].second == QString("1280x720@60"));
}

TEST_CASE("keepHighestFramerates: compressed codecs do not collapse", "[camera]")
{
  // Every compressed mode has AV_PIX_FMT_NONE as pixel format: only the codec
  // tells MJPEG and H264 apart.
  auto modes = with_descriptions(
      {mode({1920, 1080}, 30., AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE),
       mode({1920, 1080}, 5., AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE),
       mode({1920, 1080}, 25., AV_CODEC_ID_H264, AV_PIX_FMT_NONE),
       mode({1920, 1080}, 60., AV_CODEC_ID_H264, AV_PIX_FMT_NONE)});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 2);
  CHECK(modes[0].first.codec == AV_CODEC_ID_MJPEG);
  CHECK(modes[0].first.fps == 30.);
  CHECK(modes[1].first.codec == AV_CODEC_ID_H264);
  CHECK(modes[1].first.fps == 60.);
}

TEST_CASE("keepHighestFramerates: pixel formats and devices stay apart", "[camera]")
{
  auto other = mode({1920, 1080}, 10.);
  other.device = "/dev/video2";

  auto modes = with_descriptions(
      {mode({1920, 1080}, 30.),
       mode({1920, 1080}, 25., AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NV12), other});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 3);
  CHECK(modes[0].first.pixelformat == AV_PIX_FMT_YUYV422);
  CHECK(modes[1].first.pixelformat == AV_PIX_FMT_NV12);
  CHECK(modes[2].first.device == QString("/dev/video2"));
}

TEST_CASE("keepHighestFramerates: exact duplicates collapse", "[camera]")
{
  auto modes = with_descriptions(
      {mode({640, 480}, 30.), mode({640, 480}, 30.), mode({640, 480}, 30.)});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 1);
  CHECK(modes[0].first.fps == 30.);
}

TEST_CASE("keepHighestFramerates: framerates within an epsilon are one", "[camera]")
{
  // 30000/1001 announced twice with a rounding difference is one mode, not two.
  auto modes = with_descriptions(
      {mode({640, 480}, 30000. / 1001.), mode({640, 480}, 30000. / 1001. + 1e-9)});

  Gfx::keepHighestFramerates(modes);

  REQUIRE(modes.size() == 1);
  CHECK(modes[0].first.fps == 30000. / 1001.);
}

TEST_CASE("keepHighestFramerates: an empty list stays empty", "[camera]")
{
  mode_list modes;
  Gfx::keepHighestFramerates(modes);
  CHECK(modes.empty());
}

TEST_CASE("findBestCameraMode is unchanged by the reduction", "[camera]")
{
  auto modes = with_descriptions(
      {mode({640, 480}, 30.), mode({640, 480}, 60.), mode({640, 480}, 5.),
       mode({1920, 1080}, 5.), mode({1920, 1080}, 30.), mode({1920, 1080}, 60.),
       mode({1920, 1080}, 25., AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE),
       mode({1920, 1080}, 120., AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE),
       mode({3840, 2160}, 5.), mode({3840, 2160}, 10.),
       // Grayscale: never color, whatever its resolution
       mode({4096, 2160}, 90., AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY8)});

  const auto unfiltered = Gfx::findBestCameraMode(settings_of(modes));

  Gfx::keepHighestFramerates(modes);
  const auto filtered = Gfx::findBestCameraMode(settings_of(modes));

  CHECK(unfiltered.size == filtered.size);
  CHECK(unfiltered.fps == filtered.fps);
  CHECK(unfiltered.codec == filtered.codec);
  CHECK(unfiltered.pixelformat == filtered.pixelformat);

  CHECK(filtered.size == QSize{1920, 1080});
  CHECK(filtered.fps == 120.);
  CHECK(filtered.codec == AV_CODEC_ID_MJPEG);
}

TEST_CASE("findBestCameraMode: no candidate gives an empty mode", "[camera]")
{
  const auto best = Gfx::findBestCameraMode(std::vector<CameraSettings>{});
  CHECK(best.device.isEmpty());
  CHECK(best.size.isEmpty());
}
#endif

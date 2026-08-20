// Gfx/GStreamer/GStreamerDevice.cpp and GStreamerLoader.hpp: the GStreamer input
// device. Almost all of it is the pipeline the device builds and runs itself --
// gst_parse_launch, the appsink classification (video vs audio, caps, pixel
// format, size), the per-frame queue feeding a score::gfx::CameraNode, and the
// GObject property tree it publishes for every named element.
//
// None of that needs an external producer: the pipeline string IS the producer,
// so `videotestsrc` gives a known picture end to end. Each configuration is
// walked in one process, and the readback is asserted against the pattern's
// actual colour rather than against "something was rendered".
//
// No GPU and no display: the frame is scaled to a single RGBA pixel with
// sws_scale on the CPU, so the assertion happens before any upload. Registered
// through tests/hardware/with-virtual-media.sh, which requires a working
// GStreamer; the SKIP below is the last resort for a host whose libraries load
// but whose videotestsrc is missing.
//
// start_execution() must be called: reconnect() only brings the pipeline to
// GST_STATE_PAUSED, and the engine is what takes it to PLAYING, so without it
// the appsink never yields a sample.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <Gfx/GStreamer/GStreamerDevice.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/VideoNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/GfxInputDevice.hpp>

#include <Video/VideoInterface.hpp>

#include <ossia/network/base/node_functions.hpp>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace Gfx;

namespace
{
bool near(
    const std::array<uint8_t, 4>& got, const std::array<uint8_t, 4>& want, int tol)
{
  for(int i = 0; i < 4; i++)
    if(std::abs(int(got[i]) - int(want[i])) > tol)
      return false;
  return true;
}

Device::DeviceSettings gstSettings(const QString& pipeline, const QString& name)
{
  Device::DeviceSettings s;
  s.name = name;
  s.protocol = GStreamer::ProtocolFactory::static_concreteKey();
  GStreamer::GStreamerSettings gs;
  gs.pipeline = pipeline;
  gs.width = 64;
  gs.height = 64;
  gs.rate = 30;
  gs.audio_channels = 0;
  s.deviceSpecificSettings = QVariant::fromValue(gs);
  return s;
}

/// The score::gfx node an exposed texture parameter points at. Input devices
/// publish a simple_texture_input_parameter, not the gfx_parameter_base the
/// output devices use.
score::gfx::CameraNode* cameraNode(ossia::net::device_base& d, const char* path)
{
  auto* n = ossia::net::find_node(d.get_root_node(), path);
  if(!n)
    return nullptr;
  if(auto* gp = dynamic_cast<simple_texture_input_parameter*>(n->get_parameter()))
    return dynamic_cast<score::gfx::CameraNode*>(gp->node);
  return nullptr;
}

/// Pull one decoded frame out of the device's own frame queue and convert its
/// centre pixel to RGBA. No GPU involved: this is the gst pipeline -> appsink ->
/// FrameQueue -> AVFrame path end to end.
struct DecodedPixel
{
  bool ok{};
  int width{};
  int height{};
  int format{-1};
  std::array<uint8_t, 4> rgba{};
};

DecodedPixel firstFrame(Video::VideoInterface& dec, int timeoutMs = 6000)
{
  DecodedPixel out;
  QElapsedTimer t;
  t.start();
  do
  {
    if(AVFrame* f = dec.dequeue_frame())
    {
      out.width = f->width;
      out.height = f->height;
      out.format = f->format;

      if(f->width > 0 && f->height > 0)
      {
        // Convert the whole frame down to a single RGBA pixel: that averages
        // the picture, which is exactly what a solid test pattern needs.
        SwsContext* sws = sws_getContext(
            f->width, f->height, AVPixelFormat(f->format), 1, 1, AV_PIX_FMT_RGBA,
            SWS_AREA, nullptr, nullptr, nullptr);
        if(sws)
        {
          uint8_t px[4]{};
          uint8_t* dst[4]{px, nullptr, nullptr, nullptr};
          int stride[4]{4, 0, 0, 0};
          sws_scale(sws, f->data, f->linesize, 0, f->height, dst, stride);
          sws_freeContext(sws);
          out.rgba = {px[0], px[1], px[2], px[3]};
          out.ok = true;
        }
      }
      dec.release_frame(f);
      if(out.ok)
        return out;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(10);
  } while(t.elapsed() < timeoutMs);
  return out;
}

QString testsrcPipeline(const char* pattern, const char* format, bool live = true)
{
  return QStringLiteral("videotestsrc pattern=%1 is-live=%2 ! videoconvert ! "
                        "video/x-raw,format=%3,width=64,height=64,framerate=30/1 ! "
                        "appsink name=video sync=false max-buffers=2 drop=true")
      .arg(
          QString::fromUtf8(pattern), live ? QStringLiteral("true")
                                           : QStringLiteral("false"),
          QString::fromUtf8(format));
}
}

namespace
{
struct Case
{
  const char* pattern;
  const char* format;
  std::array<uint8_t, 4> expect;
  // A live source does not preroll, so its caps are unknown until the pipeline
  // reaches PLAYING; a non-live one negotiates them at PAUSED. The two take
  // different paths through the device's format classification.
  bool live{true};
};

struct Result
{
  std::string label;
  bool connected{};
  bool hasVideoNode{};
  bool decoded{};
  int width{};
  int height{};
  std::array<uint8_t, 4> got{};
};
} // namespace

namespace
{
// Runs each case through a fresh GStreamer device in one application boot.
// Returns false (with a reason) when GStreamer itself cannot be loaded here.
bool runCases(
    const std::vector<Case>& cases, std::vector<Result>& results,
    std::string& skipReason)
{
  bool ok = true;
  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      ok = false;
      skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    GStreamer::ProtocolFactory factory;

    // Probe once: if GStreamer cannot be loaded here there is nothing to say
    // about any of the cases.
    {
      std::unique_ptr<Device::DeviceInterface> probe{factory.makeDevice(
          gstSettings(testsrcPipeline("red", "RGBA"), "GstProbe"), plug,
          doc->context())};
      bool loaded = probe && probe->reconnect();
      if(loaded)
      {
        auto* d = probe->getDevice();
        loaded = d && cameraNode(*d, "/video") != nullptr;
      }
      if(probe)
        probe->disconnect();
      if(!loaded)
      {
        ok = false;
        skipReason = "the GStreamer libraries could not be loaded, or "
                     "videotestsrc is not installed";
        return;
      }
    }

    for(auto& c : cases)
    {
      Result r;
      r.label = std::string(c.pattern) + "/" + c.format;

      std::unique_ptr<Device::DeviceInterface> dev{factory.makeDevice(
          gstSettings(testsrcPipeline(c.pattern, c.format, c.live), "Gst"), plug,
          doc->context())};
      REQUIRE(dev != nullptr);
      r.connected = dev->reconnect();
      if(r.connected)
      {
        auto* d = dev->getDevice();
        auto* cam = d ? cameraNode(*d, "/video") : nullptr;
        r.hasVideoNode = cam != nullptr;
        if(cam && cam->reader.m_decoder)
        {
          // reconnect() only parses the pipeline and brings it to PAUSED; the
          // engine is what takes it to PLAYING, so without this the appsink
          // never produces a sample and every case reads back nothing.
          d->get_protocol().start_execution();
          const auto px = firstFrame(*cam->reader.m_decoder);
          r.decoded = px.ok;
          r.width = px.width;
          r.height = px.height;
          r.got = px.rgba;
          d->get_protocol().stop_execution();
        }
      }
      dev->disconnect();
      results.push_back(r);
    }
  });
  return ok;
}

void checkAll(const std::vector<Case>& cases, const std::vector<Result>& results)
{
  REQUIRE(results.size() == cases.size());
  for(std::size_t i = 0; i < results.size(); ++i)
  {
    const auto& r = results[i];
    INFO("case " << r.label << " -> " << r.width << "x" << r.height << " rgba "
                 << int(r.got[0]) << "," << int(r.got[1]) << "," << int(r.got[2]));
    CHECK(r.connected);
    CHECK(r.hasVideoNode);
    CHECK(r.decoded);
    // The caps declared 64x64 and the device must carry that through to the
    // frame it hands the renderer.
    CHECK(r.width == 64);
    CHECK(r.height == 64);
    // A YUV round trip is lossy, so allow a wide tolerance; the point is that
    // red is red and not blue.
    CHECK(near(r.got, cases[i].expect, 40));
  }
}
} // namespace

TEST_CASE("GStreamer input device decodes its pipeline", "[gfx][gstreamer][device]")
{
  // videotestsrc's solid patterns through the pixel-format families the
  // device's appsink classifier and frame queue have to handle: packed RGB
  // (padded and not), planar 8-bit YUV at three chroma subsamplings,
  // semi-planar, and packed YUV.
  const std::vector<Case> cases{
      {"red", "RGBA", {255, 0, 0, 255}},
      {"green", "RGBA", {0, 255, 0, 255}},
      {"blue", "RGBA", {0, 0, 255, 255}},
      {"white", "RGBA", {255, 255, 255, 255}},
      {"black", "RGBA", {0, 0, 0, 255}},
      {"red", "I420", {255, 0, 0, 255}},
      {"green", "I420", {0, 255, 0, 255}},
      {"blue", "NV12", {0, 0, 255, 255}},
      {"white", "NV12", {255, 255, 255, 255}},
      {"red", "UYVY", {255, 0, 0, 255}},
      {"green", "YUY2", {0, 255, 0, 255}},
      {"green", "BGRA", {0, 255, 0, 255}},
      {"blue", "BGRx", {0, 0, 255, 255}},
      {"red", "Y42B", {255, 0, 0, 255}},
      {"white", "Y444", {255, 255, 255, 255}}};

  std::vector<Result> results;
  std::string skipReason;
  if(!runCases(cases, results, skipReason))
    SKIP(skipReason);

  checkAll(cases, results);
}

// The complement of the colour sweep above: a caps format the gstreamerToLibav()
// table does not know must FAIL rather than be relabelled. Defaulting it to
// AV_PIX_FMT_RGBA fails nothing while changing the meaning of the bytes, so a
// BGRx blue frame comes back red.
//
// VYUY is a standard videotestsrc format with no row in the table, so the
// pipeline builds and only score's own classification can refuse it. Both
// classification paths are exercised: a non-live source has its caps at PAUSED
// and must be refused outright, a live one only negotiates them once running and
// must then deliver nothing.
TEST_CASE(
    "an unmapped GStreamer caps format is refused, not relabelled",
    "[gfx][gstreamer][device]")
{
  const std::vector<Case> cases{
      {"blue", "VYUY", {0, 0, 255, 255}, /*live=*/false},
      {"blue", "VYUY", {0, 0, 255, 255}, /*live=*/true}};

  std::vector<Result> results;
  std::string skipReason;
  if(!runCases(cases, results, skipReason))
    SKIP(skipReason);

  REQUIRE(results.size() == 2);
  INFO("non-live: caps are known before the device reports itself connected");
  CHECK_FALSE(results[0].connected);
  CHECK_FALSE(results[0].decoded);

  INFO("live: caps arrive only once the pipeline is PLAYING");
  CHECK_FALSE(results[1].decoded);
}

// A 9/10/12/14/16-bit PLANAR stream over the GStreamer input.
// Video::gstreamerToLibav() maps "I420_10LE" to AV_PIX_FMT_YUV420P10LE and
// score::gfx has a dedicated YUV420P10Decoder, so both ends of the path know the
// format; Video::initFrameFromRawData() in GStreamerCompatibility.hpp is the
// middle, and returning false there makes process_video_frame() drop the frame,
// leaving a device that connects, publishes /video and renders nothing.
//
// The same initFrameFromRawData() serves the Sh4lt and Shmdata inputs, so this
// is not GStreamer-specific.
TEST_CASE(
    "10-bit planar formats decode over the GStreamer input",
    "[gfx][gstreamer][device]")
{
  const std::vector<Case> cases{
      {"blue", "I420_10LE", {0, 0, 255, 255}},
      {"red", "I422_10LE", {255, 0, 0, 255}},
      {"white", "Y444_10LE", {255, 255, 255, 255}}};

  std::vector<Result> results;
  std::string skipReason;
  if(!runCases(cases, results, skipReason))
    SKIP(skipReason);

  checkAll(cases, results);
}

TEST_CASE("GStreamer input device pipeline errors", "[gfx][gstreamer][device]")
{
  bool skipped{};
  std::string skipReason;
  bool emptyConnected{true}, garbageConnected{true}, noSinkConnected{true};
  bool namedElementExposed{};
  bool twoVideoSinks{};

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      skipped = true;
      skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    {
      std::unique_ptr<Device::DeviceInterface> probe{factory.makeDevice(
          gstSettings(testsrcPipeline("red", "RGBA"), "GstProbe"), plug,
          doc->context())};
      if(!probe || !probe->reconnect())
      {
        skipped = true;
        skipReason = "the GStreamer libraries could not be loaded";
        return;
      }
      probe->disconnect();
    }

    auto tryPipeline = [&](const QString& p) {
      std::unique_ptr<Device::DeviceInterface> dev{
          factory.makeDevice(gstSettings(p, "Gst"), plug, doc->context())};
      const bool ok = dev && dev->reconnect();
      if(dev)
        dev->disconnect();
      return ok;
    };

    emptyConnected = tryPipeline(QStringLiteral(""));
    garbageConnected = tryPipeline(QStringLiteral("this is not ! a pipeline at all"));
    // Syntactically valid, but nothing for the device to read.
    noSinkConnected = tryPipeline(QStringLiteral("videotestsrc ! fakesink"));

    // A named element must publish its writable GObject properties, and two
    // video appsinks must both appear.
    {
      const auto p = QStringLiteral(
          "videotestsrc name=src0 is-live=true ! videoconvert ! "
          "video/x-raw,format=RGBA,width=32,height=32 ! appsink name=videoA "
          "sync=false "
          "videotestsrc is-live=true ! videoconvert ! "
          "video/x-raw,format=RGBA,width=32,height=32 ! appsink name=videoB "
          "sync=false");
      std::unique_ptr<Device::DeviceInterface> dev{
          factory.makeDevice(gstSettings(p, "Gst"), plug, doc->context())};
      if(dev && dev->reconnect())
      {
        auto* d = dev->getDevice();
        twoVideoSinks = ossia::net::find_node(d->get_root_node(), "/videoA")
                        && ossia::net::find_node(d->get_root_node(), "/videoB");
        // videotestsrc's "pattern" property is writable, so it must be there.
        namedElementExposed
            = ossia::net::find_node(d->get_root_node(), "/src0/pattern") != nullptr;
        dev->disconnect();
      }
    }
  });

  if(skipped)
    SKIP(skipReason);

  // A pipeline that cannot be parsed, or has no appsink to read from, must
  // fail to connect rather than half-connect.
  CHECK_FALSE(emptyConnected);
  CHECK_FALSE(garbageConnected);
  CHECK_FALSE(noSinkConnected);
  CHECK(twoVideoSinks);
  CHECK(namedElementExposed);
}

TEST_CASE("GStreamer preset enumerators", "[gfx][gstreamer][device]")
{
  int total{};
  bool allHaveAppsinkOrAppsrc{true};
  bool allNamed{true};

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    GStreamer::ProtocolFactory factory;
    auto enums = factory.getEnumerators(doc->context());
    for(auto& [category, e] : enums)
    {
      REQUIRE(e != nullptr);
      e->enumerate([&](const QString& label, const Device::DeviceSettings& s) {
        ++total;
        if(label.isEmpty() || s.name.isEmpty())
          allNamed = false;
        if(s.protocol != GStreamer::ProtocolFactory::static_concreteKey())
          allNamed = false;
        const auto gs = s.deviceSpecificSettings.value<GStreamer::GStreamerSettings>();
        // Every preset must be usable as-is: either it reads through an
        // appsink or it writes through an appsrc.
        if(!gs.pipeline.contains(QStringLiteral("appsink"))
           && !gs.pipeline.contains(QStringLiteral("appsrc")))
          allHaveAppsinkOrAppsrc = false;
      });
      delete e;
    }
  });

  CHECK(total > 0);
  CHECK(allNamed);
  CHECK(allHaveAppsinkOrAppsrc);
}

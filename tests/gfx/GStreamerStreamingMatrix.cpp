// The GStreamer input device across the pixel-format vocabulary, asserted
// against known pixels, plus its device lifecycle under hard deadlines.
//
// GStreamerDeviceRender.cpp walks videotestsrc's SOLID patterns and averages
// each frame to a single RGBA pixel, which catches a channel swap and nothing
// else: an average survives a chroma plane one column short, a plane pointer
// landing inside its neighbour, and a stride that drifts every row.
//
// So this harness pushes a PICTURE instead of a colour: the known-pixel master
// the codec matrix uses, played through
// `filesrc ! decodebin ! videoconvert ! video/x-raw,format=F`, with the
// runner's rgb24 dump as ground truth. Two geometries, 320x240 where every
// subsampling factor divides both dimensions and 65x39 where none does, since
// that is where ceil(w/2) and w/2 stop agreeing.
//
// Before any pixel is read, the published layout is checked against the size of
// the frame's own AVBufferRef: a frame whose planes do not fit is reported
// rather than converted, because reading it is the out-of-bounds access under
// test.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <Gfx/GStreamer/GStreamerDevice.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/VideoNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/VideoMaster.hpp>

#include <Video/VideoInterface.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

using namespace score::test;
using namespace score::test::video;
using namespace Gfx;

namespace
{
// The GStreamer format names Video::gstreamerToLibav() maps AND this host's
// videoconvert can produce from rgb24. A name that is not in the table would be
// refused by the device by design (see "an unmapped GStreamer caps format is
// refused" in GStreamerDeviceRender.cpp), so it has no place in a picture sweep.
const char* const kFormats[] = {
    "I420",      "NV12",      "NV21",  "NV16",  "NV24",       "YUY2",
    "UYVY",      "Y42B",      "Y444",  "RGBA",  "BGRA",       "RGB",
    "RGBx",      "BGRx",      "xRGB",  "xBGR",  "I420_10LE",  "I422_10LE",
    "Y444_10LE", "P010_10LE", "Y210",  "GBR",   "GBRA",       "A420",
    "RGBA64_LE"};

// No format loses the picture at either geometry. These stay as empty exception
// lists so that a format which starts failing is named by the one-sided guard
// rather than only counted, and so that re-admitting one is a visible edit.
const std::set<std::string> kKnownBrokenAny{};
const std::set<std::string> kKnownBrokenOddOnly{};

Device::DeviceSettings gstSettings(const QString& pipeline, const QString& name)
{
  Device::DeviceSettings s;
  s.name = name;
  s.protocol = GStreamer::ProtocolFactory::static_concreteKey();
  GStreamer::GStreamerSettings gs;
  gs.pipeline = pipeline;
  s.deviceSpecificSettings = QVariant::fromValue(gs);
  return s;
}

// The source is the ground-truth rgb24 dump itself, parsed by rawvideoparse:
// GStreamer reads exactly the bytes this harness compares against, and every
// element in the chain has STATIC pads (see the dynamic-pad case at the bottom
// of this file for why that matters).
QString rawPipeline(const std::string& raw, int w, int h, const char* format)
{
  return QStringLiteral(
             "filesrc location=%1 ! rawvideoparse width=%2 height=%3 "
             "format=rgba framerate=25/1 ! videoconvert ! "
             "video/x-raw,format=%4 ! appsink name=video sync=false "
             "max-buffers=4 drop=false")
      .arg(QString::fromStdString(raw))
      .arg(w)
      .arg(h)
      .arg(QString::fromUtf8(format));
}

// A pipeline with a DYNAMIC-pad element in it, for the defect case.
QString decodebinPipeline(const std::string& file, const char* format)
{
  return QStringLiteral(
             "filesrc location=%1 ! decodebin ! videoconvert ! "
             "video/x-raw,format=%2 ! appsink name=video sync=false "
             "max-buffers=4 drop=false")
      .arg(QString::fromStdString(file), QString::fromUtf8(format));
}

score::gfx::CameraNode* cameraNode(ossia::net::device_base& d, const char* path)
{
  auto* n = ossia::net::find_node(d.get_root_node(), path);
  if(!n)
    return nullptr;
  if(auto* gp = dynamic_cast<simple_texture_input_parameter*>(n->get_parameter()))
    return dynamic_cast<score::gfx::CameraNode*>(gp->node);
  return nullptr;
}

// Whether every plane the frame declares fits inside the buffer the frame
// carries. libav is the arithmetic: av_image_fill_plane_sizes turns the strides
// the device CHOSE into the bytes each plane then needs.
bool layoutFitsBuffer(const AVFrame& f, std::string& why)
{
  if(!f.buf[0])
  {
    why = "no AVBufferRef on the frame";
    return false;
  }
  const auto fmt = AVPixelFormat(f.format);
  const int planes = av_pix_fmt_count_planes(fmt);
  if(planes <= 0)
  {
    why = "libav does not know this pixel format";
    return false;
  }

  ptrdiff_t ls[4]{};
  for(int p = 0; p < 4; p++)
    ls[p] = f.linesize[p];
  std::size_t sizes[4]{};
  if(av_image_fill_plane_sizes(sizes, fmt, f.height, ls) < 0)
  {
    why = "av_image_fill_plane_sizes refused the strides";
    return false;
  }

  const auto* base = f.buf[0]->data;
  const std::size_t cap = f.buf[0]->size;
  bool ok = true;
  for(int p = 0; p < planes && p < 4; p++)
  {
    if(!f.data[p])
    {
      why += " plane" + std::to_string(p) + "=null";
      ok = false;
      continue;
    }
    const int minLine = av_image_get_linesize(fmt, f.width, p);
    if(minLine > 0 && f.linesize[p] < minLine)
    {
      why += " linesize" + std::to_string(p) + "=" + std::to_string(f.linesize[p])
             + "<" + std::to_string(minLine);
      ok = false;
    }
    const std::ptrdiff_t off = f.data[p] - base;
    if(off < 0 || std::size_t(off) + sizes[p] > cap)
    {
      why += " plane" + std::to_string(p) + "@" + std::to_string(off) + "+"
             + std::to_string(sizes[p]) + ">" + std::to_string(cap);
      ok = false;
    }
  }
  return ok;
}

struct FormatResult
{
  std::string format;
  bool connected{};
  bool hasVideoNode{};
  bool decoded{};
  bool layoutOk{};
  bool pictureOk{};
  int width{}, height{};
  int matchedMaster{-1};
  int maxDev{};
  std::string detail;
};

// Runs one format through a fresh GStreamer device and reports what came back.
// The document/app is the caller's: booting one per format is minutes, not
// seconds.
FormatResult runFormat(
    Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx,
    GStreamer::ProtocolFactory& factory, const char* format, const Master& m,
    const std::string& raw, int block, int margin)
{
  FormatResult r;
  r.format = format;

  std::unique_ptr<Device::DeviceInterface> dev{factory.makeDevice(
      gstSettings(rawPipeline(raw, m.width, m.height, format), "Gst"), plug, ctx)};
  if(!dev)
    return r;

  r.connected = dev->reconnect();
  if(r.connected)
  {
    auto* d = dev->getDevice();
    auto* cam = d ? cameraNode(*d, "/video") : nullptr;
    r.hasVideoNode = cam != nullptr;
    if(cam && cam->reader.m_decoder)
    {
      // reconnect() only brings the pipeline to PAUSED; the engine is what
      // takes it to PLAYING, so without this the appsink never yields.
      d->get_protocol().start_execution();

      QElapsedTimer t;
      t.start();
      int examined = 0;
      // Three frames is enough for a verdict, and bounds the cost of a format
      // that comes back wrong: without it every broken row would burn the whole
      // budget.
      while(t.elapsed() < 5000 && !r.pictureOk && examined < 3)
      {
        if(AVFrame* f = cam->reader.m_decoder->dequeue_frame())
        {
          examined++;
          r.decoded = true;
          r.width = f->width;
          r.height = f->height;

          std::string why;
          r.layoutOk = layoutFitsBuffer(*f, why);
          if(!r.layoutOk)
          {
            r.detail = why;
          }
          else
          {
            const auto best = bestMatch(*f, m, block, margin);
            r.matchedMaster = best.index;
            r.maxDev = best.maxDev;
            const bool close
                = best.index >= 0 && best.maxDev <= kToleranceYuvExact * 6;

            // The match has to be SPECIFIC, not just close: the next master
            // frame must be far away. Otherwise a picture that resembles every
            // frame equally -- a flat grey, a shear that averages out -- would
            // pass on "it matched something".
            const auto rgb = toRgb24(*f, m.width, m.height);
            double otherMean = 0.;
            if(close && rgb.size() == std::size_t(m.width) * m.height * 3)
            {
              const int other = (best.index + 1) % m.frames;
              otherMean
                  = blockDiff(
                        rgb.data(), m.frame(other), m.width, m.height, block, margin)
                        .meanDev;
            }
            r.pictureOk = close && otherMean > 8. * (best.meanDev + 1.);
            if(!r.pictureOk)
              r.detail = "best master frame " + std::to_string(best.index)
                         + " maxDev " + std::to_string(best.maxDev) + " meanDev "
                         + std::to_string(best.meanDev) + " next-frame meanDev "
                         + std::to_string(otherMean);
          }
          cam->reader.m_decoder->release_frame(f);
          if(!r.layoutOk)
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(5);
      }

      d->get_protocol().stop_execution();
    }
  }
  dev->disconnect();
  return r;
}

struct Sweep
{
  bool ran{};
  std::string skipReason;
  std::vector<FormatResult> results;
};

Sweep sweepFormats(const char* stem, int block, int margin)
{
  Sweep sweep;
  const auto m = loadMaster(stem);
  const auto raw = matrixPath((std::string{stem} + ".rgba").c_str());

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      sweep.skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    {
      std::unique_ptr<Device::DeviceInterface> probe{factory.makeDevice(
          gstSettings(rawPipeline(raw, m.width, m.height, "RGBA"), "GstProbe"),
          plug, doc->context())};
      const bool loaded = probe && probe->reconnect();
      if(probe)
        probe->disconnect();
      if(!loaded)
      {
        sweep.skipReason
            = "the GStreamer libraries could not be loaded, or this host has no "
              "decodebin/videoconvert";
        return;
      }
    }

    for(const char* f : kFormats)
      sweep.results.push_back(
          runFormat(plug, doc->context(), factory, f, m, raw, block, margin));
    sweep.ran = true;
  });
  return sweep;
}

// The two sweeps are each an application boot plus 25 pipelines; the cases
// below share one run of each rather than paying for it four times.
const Sweep& evenSweep()
{
  static const Sweep s = sweepFormats("master", blockSize(), kBlockMargin);
  return s;
}

const Sweep& oddSweep()
{
  static const Sweep s = sweepFormats("master-odd", oddBlockSize(), 3);
  return s;
}

std::string report(const std::vector<FormatResult>& rs)
{
  std::string s;
  for(const auto& r : rs)
  {
    if(r.connected && r.hasVideoNode && r.decoded && r.layoutOk && r.pictureOk)
      continue;
    s += "  " + r.format + " connected=" + std::to_string(r.connected)
         + " node=" + std::to_string(r.hasVideoNode)
         + " decoded=" + std::to_string(r.decoded)
         + " layout=" + std::to_string(r.layoutOk) + " geometry="
         + std::to_string(r.width) + "x" + std::to_string(r.height) + " {"
         + r.detail + " }\n";
  }
  return s;
}

std::set<std::string> broken(const std::vector<FormatResult>& rs)
{
  std::set<std::string> out;
  for(const auto& r : rs)
    if(!(r.connected && r.hasVideoNode && r.decoded && r.layoutOk && r.pictureOk))
      out.insert(r.format);
  return out;
}

std::string join(const std::set<std::string>& s)
{
  std::string out;
  for(const auto& x : s)
    out += x + " ";
  return out;
}
} // namespace

TEST_CASE("the GStreamer picture sweep has a picture to sweep",
          "[gfx][gstreamer][matrix][media]")
{
  // Negative control: an empty or uniform master would make every per-pixel
  // comparison below trivially satisfiable, and a comparison that cannot tell
  // two master frames apart would make "the right frame came out" vacuous.
  for(const char* stem : {"master", "master-odd"})
  {
    INFO("master " << stem);
    const auto m = loadMaster(stem);
    const int block
        = std::string{stem} == "master" ? blockSize() : oddBlockSize();
    const int margin = std::string{stem} == "master" ? kBlockMargin : 3;

    REQUIRE(m.width % block == 0);
    REQUIRE(m.height % block == 0);
    CHECK(m.frames >= 4);

    const auto same = blockDiff(m.frame(0), m.frame(0), m.width, m.height, block,
                                margin);
    CHECK(same.compared > 100);
    CHECK(same.maxDev == 0);

    int closest = 1 << 30;
    for(int i = 0; i < m.frames; i++)
      for(int j = 0; j < m.frames; j++)
        if(i != j)
          closest = std::min(
              closest,
              blockDiff(m.frame(i), m.frame(j), m.width, m.height, block, margin)
                  .maxDev);
    INFO("closest distinct master frames: " << closest);
    CHECK(closest > 80);
  }

  // ...and the odd master really is odd, which lavfi silently rounds away.
  const auto odd = loadMaster("master-odd");
  CHECK(odd.width % 2 == 1);
  CHECK(odd.height % 2 == 1);
}

TEST_CASE("no GStreamer format outside the known-broken set loses the picture",
          "[gfx][gstreamer][matrix][media]")
{
  // One-sided guard. A format that starts failing without being on a list is a
  // NEW defect and names itself here. The lists are empty, so this is the whole
  // sweep -- it stays as an exception mechanism for the day one has to be
  // re-admitted.
  {
    const auto& sweep = evenSweep();
    if(!sweep.ran)
      SKIP(sweep.skipReason);
    REQUIRE(sweep.results.size() == std::size(kFormats));

    std::set<std::string> unexpected;
    for(const auto& f : broken(sweep.results))
      if(!kKnownBrokenAny.count(f))
        unexpected.insert(f);
    INFO("at 320x240, newly broken: " << join(unexpected) << "\n"
                                      << report(sweep.results));
    CHECK(unexpected.empty());
  }
  {
    const auto& sweep = oddSweep();
    if(!sweep.ran)
      SKIP(sweep.skipReason);
    REQUIRE(sweep.results.size() == std::size(kFormats));

    std::set<std::string> unexpected;
    for(const auto& f : broken(sweep.results))
      if(!kKnownBrokenAny.count(f) && !kKnownBrokenOddOnly.count(f))
        unexpected.insert(f);
    INFO("at 65x39, newly broken: " << join(unexpected) << "\n"
                                    << report(sweep.results));
    CHECK(unexpected.empty());
  }
}

TEST_CASE("every mapped GStreamer format carries the picture at an even size",
          "[gfx][gstreamer][matrix][media]")
{
  // FINDINGS 1-3 of VideoFrameLayoutTest, end to end: NV16 and P010 used to
  // reach the renderer with a chroma plane that was never pointed anywhere, and
  // Y210 with a stride of 160 bytes for a 256-byte row. Nothing about the
  // geometry is odd here -- those three were mis-described outright.
  const auto& sweep = evenSweep();
  if(!sweep.ran)
    SKIP(sweep.skipReason);

  INFO("formats that lost the picture at 320x240:\n" << report(sweep.results));
  CHECK(broken(sweep.results).empty());
}

TEST_CASE("every mapped GStreamer format carries the picture at an odd size",
          "[gfx][gstreamer][matrix][media]")
{
  // FINDINGS 4-6, end to end. A 65x39 4:2:0 frame has 33x20 chroma planes;
  // Video::initFrameFromRawData() used to give them 32 and start plane 2 at
  // height/2 = 19 rows in, inside plane 1.
  const auto& sweep = oddSweep();
  if(!sweep.ran)
    SKIP(sweep.skipReason);

  INFO("formats that lost the picture at 65x39:\n" << report(sweep.results));
  CHECK(broken(sweep.results).empty());
}

TEST_CASE("a padded GStreamer buffer is read with the stride GStreamer chose",
          "[gfx][gstreamer][matrix][media]")
{
  // process_video_frame() in Gfx/GStreamer/GStreamerDevice.cpp sizes the frame
  // from the appsink buffer's SIZE alone and lays the planes out tightly packed.
  // GStreamer aligns a row to four bytes and publishes the real stride in the
  // buffer's GstVideoMeta, so at an odd width a 24-bit row of 65 pixels is 195
  // bytes tight and 196 as negotiated, and every row after the first shears.
  //
  // The assertion is mechanical rather than visual: the frame's own buffer is
  // LARGER than the tightly packed size, so GStreamer did pad, while the linesize
  // the device wrote is the tightly packed one. It is invisible at 320x240, where
  // 960 bytes is already aligned.
  const auto m = loadMaster("master-odd");
  const auto raw = matrixPath("master-odd.rgba");

  bool ran = false;
  std::string skipReason;
  int declaredLinesize = 0;
  int tightLinesize = 0;
  std::size_t bufferSize = 0;
  std::size_t tightSize = 0;

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    std::unique_ptr<Device::DeviceInterface> dev{factory.makeDevice(
        gstSettings(rawPipeline(raw, m.width, m.height, "RGB"), "Gst"), plug,
        doc->context())};
    if(!dev || !dev->reconnect())
    {
      skipReason = "the GStreamer libraries could not be loaded";
      return;
    }
    auto* d = dev->getDevice();
    auto* cam = d ? cameraNode(*d, "/video") : nullptr;
    if(cam && cam->reader.m_decoder)
    {
      d->get_protocol().start_execution();
      QElapsedTimer t;
      t.start();
      while(t.elapsed() < 6000 && !ran)
      {
        if(AVFrame* f = cam->reader.m_decoder->dequeue_frame())
        {
          declaredLinesize = f->linesize[0];
          tightLinesize
              = av_image_get_linesize(AVPixelFormat(f->format), f->width, 0);
          bufferSize = f->buf[0] ? f->buf[0]->size : 0;
          tightSize = std::size_t(av_image_get_buffer_size(
              AVPixelFormat(f->format), f->width, f->height, 1));
          ran = true;
          cam->reader.m_decoder->release_frame(f);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(5);
      }
      d->get_protocol().stop_execution();
    }
    dev->disconnect();
  });

  if(!ran)
  {
    const std::string why
        = skipReason.empty() ? std::string{"no frame was produced"} : skipReason;
    SKIP(why);
  }

  INFO("buffer " << bufferSize << " bytes for a tightly packed " << tightSize
                 << ", linesize " << declaredLinesize << " vs tight "
                 << tightLinesize);
  // GStreamer really did pad -- otherwise there is nothing to get wrong.
  REQUIRE(bufferSize > tightSize);
  // ...and the device must have used the stride that padding implies.
  CHECK(declaredLinesize > tightLinesize);
}

TEST_CASE("a pipeline with a dynamic-pad element still delivers frames",
          "[gfx][gstreamer][matrix][media]")
{
  // gstreamer_pipeline::load() returns the pipeline to GST_STATE_NULL after
  // probing the caps at PAUSED. For a gst_parse_launch pipeline containing an
  // element with DYNAMIC pads -- decodebin, uridecodebin, rtspsrc, tsdemux,
  // qtdemux -- the parse-launch delayed link is one-shot: it fires on the first
  // pad-added and disconnects. Cycling to NULL destroys those pads, the next
  // PLAYING creates new ones nothing links, the state change stays ASYNC forever
  // and the appsink never produces a sample. The device still reports itself
  // connected and still publishes /video.
  //
  // Reproduced outside score with plain GStreamer: PAUSED -> PLAYING yields
  // samples, PAUSED -> NULL -> PLAYING yields none. It affects the shipped presets
  // "Video file", "Video file + audio", "RTSP stream", "SRT receiver" and "WHEP
  // receiver". The static-pad control in the same case is what makes this a
  // statement about dynamic pads rather than about file sources.
  const auto m = loadMaster();
  const auto mkv = matrixPath("master.mkv");
  const auto raw = matrixPath("master.rgba");

  bool ran = false;
  std::string skipReason;
  bool dynamicConnected = false, dynamicDecoded = false;
  bool staticConnected = false, staticDecoded = false;

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    auto drive = [&](const QString& pipeline, bool& connected, bool& decoded) {
      std::unique_ptr<Device::DeviceInterface> dev{
          factory.makeDevice(gstSettings(pipeline, "Gst"), plug, doc->context())};
      if(!dev)
        return;
      connected = dev->reconnect();
      if(connected)
      {
        auto* d = dev->getDevice();
        auto* cam = d ? cameraNode(*d, "/video") : nullptr;
        if(cam && cam->reader.m_decoder)
        {
          d->get_protocol().start_execution();
          QElapsedTimer t;
          t.start();
          while(t.elapsed() < 6000 && !decoded)
          {
            if(AVFrame* f = cam->reader.m_decoder->dequeue_frame())
            {
              decoded = true;
              cam->reader.m_decoder->release_frame(f);
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            QThread::msleep(5);
          }
          d->get_protocol().stop_execution();
        }
      }
      dev->disconnect();
    };

    drive(rawPipeline(raw, m.width, m.height, "RGBA"), staticConnected,
          staticDecoded);
    drive(decodebinPipeline(mkv, "RGBA"), dynamicConnected, dynamicDecoded);
    ran = true;
  });

  if(!ran)
  {
    const std::string why
        = skipReason.empty() ? std::string{"the GStreamer device could not be "
                                           "exercised"}
                             : skipReason;
    SKIP(why);
  }

  // The control: a pipeline whose elements all have static pads works, so the
  // failure below is not "score cannot read a file".
  REQUIRE(staticConnected);
  REQUIRE(staticDecoded);

  // The device says it is connected either way.
  CHECK(dynamicConnected);
  // ...but only one of the two ever produces a frame.
  CHECK(dynamicDecoded);
}

// ---------------------------------------------------------------------------
// Device lifecycle. Every case is bounded by a wall-clock budget checked from
// the calling thread: the GStreamer device does its teardown synchronously
// inside disconnect()/~InputDevice(), so a hang shows up as a budget overrun
// rather than as a deadlock the harness cannot report.

namespace
{
struct LifecycleReport
{
  bool ran{};
  std::string skipReason;
  std::vector<std::pair<std::string, qint64>> timings;
  std::string failure;
};

qint64 timeIt(auto&& fn)
{
  QElapsedTimer t;
  t.start();
  fn();
  return t.elapsed();
}
} // namespace

TEST_CASE("the GStreamer device survives adversarial lifecycles",
          "[gfx][gstreamer][matrix][media][lifecycle]")
{
  // Nothing here may hang. The budgets are deliberately generous (seconds for
  // work that takes milliseconds) so that a failure means "wedged", not "slow
  // machine".
  LifecycleReport rep;
  const auto masterGeom = loadMaster();
  const auto rawMaster = matrixPath("master.rgba");
  const auto live
      = QStringLiteral("videotestsrc is-live=true pattern=smpte ! videoconvert ! "
                       "video/x-raw,format=RGBA,width=64,height=64 ! "
                       "appsink name=video sync=false max-buffers=2 drop=true");

  run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = new_document(ctx);
    if(!doc)
    {
      rep.skipReason = "no document delegate";
      return;
    }
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    auto make = [&](const QString& pipeline) {
      return std::unique_ptr<Device::DeviceInterface>{
          factory.makeDevice(gstSettings(pipeline, "Gst"), plug, doc->context())};
    };

    {
      auto probe = make(live);
      const bool loaded = probe && probe->reconnect();
      if(probe)
        probe->disconnect();
      if(!loaded)
      {
        rep.skipReason = "the GStreamer libraries could not be loaded";
        return;
      }
    }

    // Created and destroyed without ever connecting.
    rep.timings.emplace_back("create/destroy, never connected", timeIt([&] {
                               for(int i = 0; i < 20; i++)
                                 auto dev = make(live);
                             }));

    // Connected and destroyed without ever being started.
    rep.timings.emplace_back("connect/destroy, never started", timeIt([&] {
                               for(int i = 0; i < 10; i++)
                               {
                                 auto dev = make(live);
                                 dev->reconnect();
                               }
                             }));

    // Destroyed while the pipeline is PLAYING and producing.
    rep.timings.emplace_back("destroy while streaming", timeIt([&] {
                               for(int i = 0; i < 5; i++)
                               {
                                 auto dev = make(live);
                                 if(dev->reconnect())
                                 {
                                   if(auto* d = dev->getDevice())
                                   {
                                     d->get_protocol().start_execution();
                                     QThread::msleep(120);
                                   }
                                 }
                               }
                             }));

    // Destroyed before the first frame: a file source that has not prerolled
    // yet, torn down immediately after start.
    rep.timings.emplace_back("destroy before the first frame", timeIt([&] {
                               for(int i = 0; i < 5; i++)
                               {
                                 auto dev = make(rawPipeline(
                                     rawMaster, masterGeom.width,
                                     masterGeom.height, "RGBA"));
                                 if(dev->reconnect())
                                   if(auto* d = dev->getDevice())
                                     d->get_protocol().start_execution();
                               }
                             }));

    // start_execution / stop_execution in the orders a transport can produce.
    rep.timings.emplace_back("degenerate start/stop orders", timeIt([&] {
                               auto dev = make(live);
                               if(!dev->reconnect())
                               {
                                 rep.failure = "the device did not connect";
                                 return;
                               }
                               auto* d = dev->getDevice();
                               if(!d)
                               {
                                 rep.failure = "the device published no ossia device";
                                 return;
                               }
                               auto& proto = d->get_protocol();
                               proto.stop_execution();
                               proto.stop_execution();
                               proto.start_execution();
                               QThread::msleep(80);
                               proto.stop_execution();
                               proto.stop_execution();
                               proto.start_execution();
                               QThread::msleep(80);
                               proto.stop_execution();
                               dev->disconnect();
                               dev->disconnect();
                             }));

    // Two devices on one pipeline, torn down in the opposite order.
    rep.timings.emplace_back("two devices on one pipeline", timeIt([&] {
                               auto a = make(live);
                               auto b = make(live);
                               const bool ca = a->reconnect();
                               const bool cb = b->reconnect();
                               if(ca)
                                 if(auto* d = a->getDevice())
                                   d->get_protocol().start_execution();
                               if(cb)
                                 if(auto* d = b->getDevice())
                                   d->get_protocol().start_execution();
                               QThread::msleep(150);
                               a.reset();
                               QThread::msleep(80);
                               b.reset();
                             }));

    // reconnect() on a live device: the old pipeline has to be released before
    // the new one is built.
    rep.timings.emplace_back("reconnect while streaming", timeIt([&] {
                               auto dev = make(live);
                               for(int i = 0; i < 5; i++)
                               {
                                 if(dev->reconnect())
                                   if(auto* d = dev->getDevice())
                                     d->get_protocol().start_execution();
                                 QThread::msleep(60);
                               }
                             }));

    // A pipeline that cannot be built, repeatedly: the failure path allocates
    // and frees the same objects as the success path.
    rep.timings.emplace_back("rapid failing connects", timeIt([&] {
                               for(int i = 0; i < 20; i++)
                               {
                                 auto dev = make(QStringLiteral(
                                     "this is not ! a pipeline at all"));
                                 dev->reconnect();
                               }
                             }));

    rep.ran = true;
  });

  if(!rep.ran)
  {
    const std::string why = rep.skipReason.empty()
                                ? std::string{"the GStreamer device could not be "
                                              "exercised"}
                                : rep.skipReason;
    SKIP(why);
  }

  CHECK(rep.failure.empty());

  // Budgets, in milliseconds. Each is at least ten times the observed cost.
  const std::vector<std::pair<std::string, qint64>> budgets{
      {"create/destroy, never connected", 20000},
      {"connect/destroy, never started", 60000},
      {"destroy while streaming", 60000},
      {"destroy before the first frame", 60000},
      {"degenerate start/stop orders", 60000},
      {"two devices on one pipeline", 60000},
      {"reconnect while streaming", 60000},
      {"rapid failing connects", 30000}};

  REQUIRE(rep.timings.size() == budgets.size());
  for(std::size_t i = 0; i < budgets.size(); i++)
  {
    INFO(rep.timings[i].first << " took " << rep.timings[i].second << " ms (budget "
                              << budgets[i].second << ")");
    CHECK(rep.timings[i].first == budgets[i].first);
    CHECK(rep.timings[i].second < budgets[i].second);
  }
}

// Every pipeline the GStreamer protocol SHIPS as a preset, driven for real, in
// both directions.
//
// The presets are the only pipelines a user meets without writing one, so they
// are the pipelines that have to work. They are enumerated through
// ProtocolFactory::getEnumerators() rather than copied, so a preset that is
// added, edited or platform-gated is covered automatically and cannot quietly
// go untested.
//
// Wherever the shipped set contains a SENDER and a RECEIVER for the same
// protocol, they are run against each other on loopback with the known master
// picture on the sending side. That is a stronger statement than probing either
// half alone and it needs no external server: score's device receives, the
// shipped output preset sends, and the pixels are compared 1:1.
//
// Verdicts, one per preset and never none:
//
//   WORKS  - driven end to end and the PIXELS were compared against a known
//            picture (or, for an output preset, what it wrote was decoded back
//            and compared). Connecting is never enough for this verdict.
//   built  - constructed and connected for real, so every element exists and
//            the pipeline negotiated, but this harness has no oracle for its
//            content. The reason is recorded per preset.
//   SKIP   - cannot run here. The reason names the missing element, device,
//            kernel module or peer exactly.
//   WEDGED - it had to be killed. That is a finding, not an absence.
//
// Every preset is exercised by a fresh copy of this binary under a wall-clock
// deadline, so a device that never returns is attributed to the preset that
// opened it instead of taking the sweep down with it.

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QThread>
#include <QUdpSocket>

#include <Gfx/GStreamer/GStreamerDevice.hpp>
#include <Gfx/GStreamer/GStreamerLoader.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/VideoNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/VideoMaster.hpp>

#include <Video/VideoInterface.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

using namespace score::test;
using namespace score::test::video;
using namespace Gfx;

namespace
{

struct Preset
{
  QString group;
  QString label;
  QString direction;
  QString pipeline;
};

// ---------------------------------------------------------------------------
// Host inventory

bool gstreamerUsable()
{
  auto& gst = GStreamer::libgstreamer::instance();
  return gst.available && GStreamer::gstreamer_init() && gst.parse_launch
         && gst.object_unref && gst.element_set_state;
}

// What gst_parse_launch said when it refused, or an empty string when the
// description builds. This separates "this host has no x264enc" from "this
// preset is malformed".
// GstParseError, from gst/gstparse.h. Only NO_SUCH_ELEMENT is a property of
// the host; everything else is a preset we shipped wrong.
enum
{
  kParseSyntax = 0,
  kParseNoSuchElement = 1,
  kParseNoSuchProperty = 2,
  kParseLink = 3,
  kParseCouldNotSetProperty = 4,
  kParseEmptyBin = 5,
  kParseEmpty = 6,
  kParseDelayedLink = 7
};

struct ParseResult
{
  bool failed{};
  int code{-1};
  std::string message;
};

ParseResult parseCheck(const QString& pipeline)
{
  auto& gst = GStreamer::libgstreamer::instance();
  GError* err{};
  const auto utf8 = pipeline.toStdString();
  GstElement* p = gst.parse_launch(utf8.c_str(), &err);
  ParseResult r;
  r.failed = err != nullptr || p == nullptr;
  if(err)
  {
    r.code = err->code;
    r.message = err->message ? err->message : "";
    if(gst.g_error_free)
      gst.g_error_free(err);
  }
  if(p)
  {
    gst.element_set_state(p, GST_STATE_NULL);
    gst.object_unref(p);
  }
  if(r.failed && r.message.empty())
    r.message = "gst_parse_launch refused the pipeline";
  return r;
}

// The message when it failed, empty when it built.
std::string parseFailure(const QString& pipeline)
{
  const auto r = parseCheck(pipeline);
  return r.failed ? r.message : std::string{};
}

bool hasElement(const char* name)
{
  static std::map<std::string, bool> cache;
  if(auto it = cache.find(name); it != cache.end())
    return it->second;
  const bool ok = parseFailure(QString::fromUtf8(name)).empty();
  cache[name] = ok;
  return ok;
}

// The H.264 encoder this host has. The presets name x264enc, which lives in
// gst-plugins-ugly and is often absent; a substitute peer only has to produce
// H.264, it does not have to be the preset.
const char* h264Encoder()
{
  for(const char* e : {"x264enc", "openh264enc", "nvh264enc"})
    if(hasElement(e))
      return e;
  return nullptr;
}

int freeUdpPort()
{
  QUdpSocket s;
  if(!s.bind(QHostAddress::LocalHost, 0))
    return 0;
  const int port = s.localPort();
  s.close();
  return port;
}

// RIST binds port and port+1 (RTP and RTCP) and refuses an odd one outright:
// "Invalid RIST port N, should be an even number".
int freeEvenUdpPort()
{
  for(int i = 0; i < 32; i++)
  {
    const int p = freeUdpPort();
    if(p > 0 && p % 2 == 0)
      return p;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Verdicts

enum class Verdict
{
  Works,
  Built,
  Skipped,
  Wedged
};

const char* toString(Verdict v)
{
  switch(v)
  {
    case Verdict::Works:
      return "WORKS ";
    case Verdict::Built:
      return "built ";
    case Verdict::Skipped:
      return "SKIP  ";
    case Verdict::Wedged:
      return "WEDGED";
  }
  return "?";
}

Verdict fromChar(char c)
{
  switch(c)
  {
    case 'W':
      return Verdict::Works;
    case 'B':
      return Verdict::Built;
    case 'X':
      return Verdict::Wedged;
    default:
      return Verdict::Skipped;
  }
}

char toChar(Verdict v)
{
  switch(v)
  {
    case Verdict::Works:
      return 'W';
    case Verdict::Built:
      return 'B';
    case Verdict::Wedged:
      return 'X';
    case Verdict::Skipped:
      return 'S';
  }
  return 'S';
}

struct Report
{
  Preset preset;
  Verdict verdict{Verdict::Skipped};
  std::string detail;
};

std::string render(const std::vector<Report>& rs)
{
  std::string out = "\n";
  for(const auto& r : rs)
    out += std::string{toString(r.verdict)} + "  " + r.preset.group.toStdString()
           + " / " + r.preset.label.toStdString() + " ["
           + r.preset.direction.toStdString() + "]\n            " + r.detail + "\n";
  return out;
}

struct Outcome
{
  Verdict verdict{Verdict::Skipped};
  std::string detail;
};

// ---------------------------------------------------------------------------
// One preset per process.
//
// Half of what is under test opens a real device, socket or kernel module, and
// any of those can fail to return. Each preset is therefore exercised by a
// fresh copy of this binary, run under a wall-clock deadline: a preset that
// never comes back is attributed to itself instead of taking the sweep down.
//
// A fresh PROCESS rather than a fork: gst_init() has already run here by the
// time the sweep starts, and a forked child inherits its global state without
// the threads that own it.

constexpr auto kProbeCase = "the GStreamer preset probe";
constexpr int kPresetBudgetMs = 150000;
constexpr auto kIndexEnv = "SCORE_GST_PRESET_INDEX";
constexpr auto kOutEnv = "SCORE_GST_PRESET_OUT";

// ---------------------------------------------------------------------------
// Driving a preset through the real device

score::gfx::CameraNode* cameraNode(ossia::net::device_base& d, const char* path)
{
  auto* n = ossia::net::find_node(d.get_root_node(), path);
  if(!n)
    return nullptr;
  if(auto* gp = dynamic_cast<simple_texture_input_parameter*>(n->get_parameter()))
    return dynamic_cast<score::gfx::CameraNode*>(gp->node);
  return nullptr;
}

Device::DeviceSettings gstSettings(const QString& pipeline)
{
  Device::DeviceSettings s;
  s.name = QStringLiteral("Gst");
  s.protocol = GStreamer::ProtocolFactory::static_concreteKey();
  GStreamer::GStreamerSettings gs;
  gs.pipeline = pipeline;
  s.deviceSpecificSettings = QVariant::fromValue(gs);
  return s;
}

struct Frames
{
  bool connected{};
  bool hasVideo{};
  bool hasAudio{};
  int decoded{};
  int width{}, height{};
  std::vector<std::vector<uint8_t>> rgb; // rgb24 at the master's geometry
  std::vector<std::vector<uint8_t>> raw; // RGBA, rows repacked tightly
};

struct DriveOptions
{
  int wanted{3};
  int budgetMs{8000};
  int rgbW{}, rgbH{};
  bool collectRaw{};
};

std::vector<uint8_t> packRows(const AVFrame& f, int bytesPerRow)
{
  std::vector<uint8_t> out(std::size_t(bytesPerRow) * f.height);
  for(int y = 0; y < f.height; y++)
    std::memcpy(
        out.data() + std::size_t(y) * bytesPerRow,
        f.data[0] + std::ptrdiff_t(y) * f.linesize[0], bytesPerRow);
  return out;
}

Frames drive(
    Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx,
    GStreamer::ProtocolFactory& factory, const QString& pipeline,
    const DriveOptions& opt)
{
  Frames f;
  std::unique_ptr<Device::DeviceInterface> dev{
      factory.makeDevice(gstSettings(pipeline), plug, ctx)};
  if(!dev)
    return f;

  f.connected = dev->reconnect();
  if(f.connected)
  {
    if(auto* d = dev->getDevice())
    {
      f.hasAudio = ossia::net::find_node(d->get_root_node(), "/audio") != nullptr;
      auto* cam = cameraNode(*d, "/video");
      f.hasVideo = cam != nullptr;
      if(cam && cam->reader.m_decoder)
      {
        d->get_protocol().start_execution();
        QElapsedTimer t;
        t.start();
        while(t.elapsed() < opt.budgetMs && f.decoded < opt.wanted)
        {
          if(AVFrame* fr = cam->reader.m_decoder->dequeue_frame())
          {
            f.decoded++;
            f.width = fr->width;
            f.height = fr->height;
            if(opt.rgbW > 0 && opt.rgbH > 0)
            {
              auto rgb = toRgb24(*fr, opt.rgbW, opt.rgbH);
              if(rgb.size() == std::size_t(opt.rgbW) * opt.rgbH * 3)
                f.rgb.push_back(std::move(rgb));
            }
            if(opt.collectRaw && fr->data[0] && fr->format == AV_PIX_FMT_RGBA)
              f.raw.push_back(packRows(*fr, fr->width * 4));
            cam->reader.m_decoder->release_frame(fr);
          }
          QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
          QThread::msleep(5);
        }
        d->get_protocol().stop_execution();
      }
      else if(f.hasAudio)
      {
        d->get_protocol().start_execution();
        QThread::msleep(400);
        d->get_protocol().stop_execution();
      }
    }
  }
  dev->disconnect();
  return f;
}

// ---------------------------------------------------------------------------
// Comparing against the master

struct Match
{
  int index{-1};
  int maxDev{1 << 30};
  double meanDev{};
  double nextMeanDev{};
};

Match matchOne(
    const std::vector<uint8_t>& rgb, const Master& m, int block, int margin)
{
  Match best;
  for(int j = 0; j < m.frames; j++)
  {
    const auto d
        = blockDiff(rgb.data(), m.frame(j), m.width, m.height, block, margin);
    if(d.compared > 0 && d.maxDev < best.maxDev)
    {
      best.index = j;
      best.maxDev = d.maxDev;
      best.meanDev = d.meanDev;
      best.nextMeanDev = blockDiff(
                             rgb.data(), m.frame((j + 1) % m.frames), m.width,
                             m.height, block, margin)
                             .meanDev;
    }
  }
  return best;
}

Match bestOf(const Frames& f, const Master& m, int block, int margin)
{
  Match best;
  for(const auto& rgb : f.rgb)
  {
    const auto one = matchOne(rgb, m, block, margin);
    if(one.index >= 0 && one.maxDev < best.maxDev)
      best = one;
  }
  return best;
}

// Close to ONE master frame and far from the next, so a flat or sheared frame
// that resembles everything equally cannot pass on "it matched something".
bool carriesThePicture(const Match& m, int tolerance, std::string& why)
{
  if(m.index < 0)
  {
    why = "no frame could be converted for comparison";
    return false;
  }
  if(m.maxDev > tolerance || m.nextMeanDev <= 4. * (m.meanDev + 1.))
  {
    why = "best master frame " + std::to_string(m.index) + ", maxDev "
          + std::to_string(m.maxDev) + " (tolerance " + std::to_string(tolerance)
          + "), meanDev " + std::to_string(m.meanDev) + ", next-frame meanDev "
          + std::to_string(m.nextMeanDev);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Peers

// Turns a shipped OUTPUT preset into a runnable sender by replacing the appsrc
// the gfx graph would feed with a loop of the known master. Everything after
// that -- the encoder, the payloader, the sink and all their options -- is the
// preset exactly as it ships, which is what makes a pair test a statement about
// BOTH presets.
QString senderFromOutputPreset(const Master& m, const std::string& raw, QString out)
{
  const QString src
      = QStringLiteral(
            "multifilesrc location=%1 loop=true ! rawvideoparse width=%2 "
            "height=%3 format=rgba framerate=25/1 ! ")
            .arg(QString::fromStdString(raw))
            .arg(m.width)
            .arg(m.height);
  out.replace(QStringLiteral("appsrc name=video ! "), src);
  out.replace(
      QStringLiteral("appsrc name=audio ! "),
      QStringLiteral("audiotestsrc is-live=true wave=sine freq=440 ! "));
  return out;
}

// gst-launch-1.0 parses its ARGV, one token per argument: a whole description
// handed over as a single argument is a syntax error. Nothing in a pipeline
// description contains a space that matters, so splitting on whitespace is what
// the shell would have done.
QStringList gstArgs(const QString& description, bool eos = true)
{
  QStringList args;
  if(eos)
    args << QStringLiteral("-e");
  args << description.split(QRegularExpression{QStringLiteral("\\s+")},
                            Qt::SkipEmptyParts);
  return args;
}

struct Peer
{
  explicit Peer(const QString& description)
  {
    proc.start(QStringLiteral("gst-launch-1.0"), gstArgs(description));
    started = proc.waitForStarted(5000);
  }
  ~Peer()
  {
    proc.kill();
    proc.waitForFinished(3000);
  }
  // gst-launch finalises a container on SIGINT when it was given -e; SIGTERM
  // just kills it and leaves the file unmuxed.
  void interrupt()
  {
#if !defined(_WIN32)
    if(proc.processId() > 0)
      ::kill(proc.processId(), SIGINT);
#else
    proc.terminate();
#endif
  }

  // What gst-launch said, for a failure report: a peer that refused to
  // negotiate is the answer to "why did nothing arrive".
  std::string diagnostics()
  {
    const QByteArray err = proc.readAllStandardError();
    if(err.isEmpty())
      return {};
    return " peer said: " + QString::fromUtf8(err).simplified().left(400).toStdString();
  }

  QProcess proc;
  bool started{};
};

// ---------------------------------------------------------------------------
// Reading back what an output preset wrote

// Decodes a media file with libav and returns its frames as rgb24 at the
// master's geometry.
std::vector<std::vector<uint8_t>> decodeFile(const std::string& path, const Master& m)
{
  std::vector<std::vector<uint8_t>> out;
  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) != 0)
    return out;
  if(avformat_find_stream_info(fmt, nullptr) < 0)
  {
    avformat_close_input(&fmt);
    return out;
  }
  int vs = -1;
  for(unsigned i = 0; i < fmt->nb_streams; i++)
    if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      vs = int(i);
      break;
    }
  if(vs < 0)
  {
    avformat_close_input(&fmt);
    return out;
  }

  const AVCodec* dec = avcodec_find_decoder(fmt->streams[vs]->codecpar->codec_id);
  AVCodecContext* cc = dec ? avcodec_alloc_context3(dec) : nullptr;
  if(!cc || avcodec_parameters_to_context(cc, fmt->streams[vs]->codecpar) < 0
     || avcodec_open2(cc, dec, nullptr) < 0)
  {
    if(cc)
      avcodec_free_context(&cc);
    avformat_close_input(&fmt);
    return out;
  }

  AVPacket* pkt = av_packet_alloc();
  AVFrame* frame = av_frame_alloc();
  while(out.size() < 8 && av_read_frame(fmt, pkt) >= 0)
  {
    if(pkt->stream_index == vs && avcodec_send_packet(cc, pkt) == 0)
    {
      while(avcodec_receive_frame(cc, frame) == 0 && out.size() < 8)
      {
        auto rgb = toRgb24(*frame, m.width, m.height);
        if(rgb.size() == std::size_t(m.width) * m.height * 3)
          out.push_back(std::move(rgb));
      }
    }
    av_packet_unref(pkt);
  }
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&cc);
  avformat_close_input(&fmt);
  return out;
}

Match bestOfDecoded(
    const std::vector<std::vector<uint8_t>>& frames, const Master& m, int block,
    int margin)
{
  Match best;
  for(const auto& rgb : frames)
  {
    const auto one = matchOne(rgb, m, block, margin);
    if(one.index >= 0 && one.maxDev < best.maxDev)
      best = one;
  }
  return best;
}

// In the probe process the wall-clock deadline is the parent's, so this is
// just a call. The signature is kept so that each preset's own budget stays
// written next to it.
template <typename F>
Outcome runBounded(int, const QString&, F&& fn)
{
  return fn();
}

// How a probe hands its verdict back: one character, the detail, and
// optionally the sender preset the run also proved.
constexpr char kProvenSep = '\x01';
constexpr char kDetailSep = '\x02';

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE("the GStreamer preset probe", "[.probe]")
{
  const auto m = loadMaster();
  const auto rawMaster = matrixPath("master.rgba");
  const auto mkvMaster = matrixPath("master.mkv");
  const int block = blockSize();
  const QString scratch = QDir::tempPath() + QStringLiteral("/score-gst-presets");
  QDir{}.mkpath(scratch);

  std::vector<Report> reports;
  std::string skipReason;
  bool ran = false;
  QString provenSenderLabel;
  std::string provenSenderDetail;

  run_in_app([&](const score::GUIApplicationContext& appctx) {
    auto* doc = new_document(appctx);
    if(!doc)
    {
      skipReason = "no document delegate";
      return;
    }
    const auto& ctx = doc->context();
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    GStreamer::ProtocolFactory factory;

    if(!gstreamerUsable())
    {
      skipReason = "the GStreamer libraries could not be loaded";
      return;
    }

    // --- enumerate, so that nothing shipped can be missed -------------------
    std::vector<Preset> shipped;
    {
      auto enums = factory.getEnumerators(ctx);
      for(auto& [group, e] : enums)
      {
        e->enumerate(
            [&, g = group](const QString& label, const Device::DeviceSettings& s) {
              const auto gs
                  = s.deviceSpecificSettings.value<GStreamer::GStreamerSettings>();
              shipped.push_back({g, label, s.name, gs.pipeline});
            });
        delete e;
      }
    }
    REQUIRE(shipped.size() >= 30);

    // One preset per process: the parent owns the deadline and the report.
    bool ok = false;
    const int only = qgetenv(kIndexEnv).toInt(&ok);
    REQUIRE(ok);
    REQUIRE(only >= 0);
    REQUIRE(only < int(shipped.size()));

    // The shipped OUTPUT preset that sends what a given input preset receives.
    const std::map<QString, QString> pairs{
        {QStringLiteral("UDP MJPEG receiver"), QStringLiteral("UDP MJPEG streaming")},
        {QStringLiteral("UDP H.264 receiver"), QStringLiteral("UDP H.264 streaming")},
        {QStringLiteral("SRT receiver"), QStringLiteral("SRT H.264 streaming")},
        {QStringLiteral("RIST receiver"), QStringLiteral("RIST H.264 streaming")},
        {QStringLiteral("WHEP receiver (WebRTC)"),
         QStringLiteral("WHIP sender (WebRTC)")},
        {QStringLiteral("ST 2110-20 video receiver (raw)"),
         QStringLiteral("ST 2110-20 video sender (raw)")},
        {QStringLiteral("ST 2110-30 audio receiver (L16 48kHz stereo)"),
         QStringLiteral("ST 2110-30 audio sender (L16 48kHz stereo)")},
        {QStringLiteral("ST 2022-1 H.264 receiver (FEC)"),
         QStringLiteral("ST 2022-1 H.264 sender (FEC)")}};

    std::map<QString, QString> byLabel;
    for(const auto& p : shipped)
      byLabel[p.label] = p.pipeline;

    const Preset selected = shipped[only];
    shipped.assign(1, selected);

    auto note = [&](const Preset& p, Verdict v, std::string detail) {
      reports.push_back({p, v, detail});
      std::fprintf(
          stderr, "[preset] %s  %s / %s  %s\n", toString(v),
          p.group.toStdString().c_str(), p.label.toStdString().c_str(),
          detail.c_str());
      std::fflush(stderr);
    };

    for(const auto& p : shipped)
    {
      // 1. Does the description build on this host at all?
      if(const auto build = parseCheck(p.pipeline); build.failed)
      {
        note(p, Verdict::Skipped,
             "[gst-parse-error " + std::to_string(build.code) + "] "
                 + build.message);
        continue;
      }

      const bool isInput = !p.pipeline.contains(QStringLiteral("appsrc"));
      const bool isVideoInput
          = isInput && p.pipeline.contains(QStringLiteral("appsink name=video"));

      // -------------------------------------------------------------------
      // Input: a file through decodebin's dynamic pads.
      if(p.label == "Video file" || p.label == "Video file + audio")
      {
        const auto out = runBounded(60, QStringLiteral("file"), [&]() -> Outcome {
          // The audio variant demuxes two branches, and an appsink that never
          // gets a pad stops the whole pipeline prerolling -- so it needs a
          // clip that really has both.
          QString source = QString::fromStdString(mkvMaster);
          if(p.label.contains(QStringLiteral("audio")))
          {
            source = scratch + QStringLiteral("/master-av.mkv");
            QFile::remove(source);
            QProcess mux;
            mux.start(
                QStringLiteral("ffmpeg"),
                {QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
                 QStringLiteral("error"), QStringLiteral("-y"),
                 QStringLiteral("-i"), QString::fromStdString(mkvMaster),
                 QStringLiteral("-f"), QStringLiteral("lavfi"),
                 QStringLiteral("-i"),
                 QStringLiteral("sine=frequency=440:sample_rate=48000"),
                 QStringLiteral("-shortest"), QStringLiteral("-c:v"),
                 QStringLiteral("copy"), QStringLiteral("-c:a"),
                 QStringLiteral("pcm_s16le"), source});
            mux.waitForFinished(60000);
            if(!QFile::exists(source) || QFileInfo{source}.size() == 0)
              return {Verdict::Skipped,
                      "ffmpeg could not mux an audio track onto the master"};
          }
          QString pipeline = p.pipeline;
          pipeline.replace(QStringLiteral("/path/to/video.mp4"), source);
          const auto f
              = drive(plug, ctx, factory, pipeline, {3, 8000, m.width, m.height});
          std::string why;
          const auto best = bestOf(f, m, block, kBlockMargin);
          const bool wantsAudio = p.label.contains(QStringLiteral("audio"));
          if(f.connected && f.hasVideo && (!wantsAudio || f.hasAudio)
             && carriesThePicture(best, kToleranceYuvExact * 6, why))
            return {Verdict::Works,
                    "decoded " + std::to_string(f.decoded)
                        + " frames through decodebin's dynamic pads; matched "
                          "master frame "
                        + std::to_string(best.index) + " to within "
                        + std::to_string(best.maxDev)
                        + (wantsAudio ? "; /audio published from the second "
                                        "demux branch"
                                      : "")};
          return {Verdict::Skipped,
                  "connected=" + std::to_string(f.connected)
                      + " video=" + std::to_string(f.hasVideo)
                      + " frames=" + std::to_string(f.decoded) + " " + why};
        });
        note(p, out.verdict, out.detail);
        continue;
      }

      // -------------------------------------------------------------------
      // Input: the deterministic generator, against a dump of itself.
      if(p.label == "Test pattern" || p.label == "Test pattern + tone")
      {
        const auto out = runBounded(90, QStringLiteral("tp"), [&]() -> Outcome {
          const QString ref = scratch + QStringLiteral("/videotestsrc.rgba");
          QFile::remove(ref);
          QProcess dump;
          dump.start(
              QStringLiteral("gst-launch-1.0"),
              gstArgs(
                  QStringLiteral("videotestsrc num-buffers=6 ! videoconvert ! "
                                 "video/x-raw,format=RGBA ! filesink location=")
                      + ref,
                  false));
          dump.waitForFinished(30000);

          QFile rf{ref};
          QByteArray refBytes;
          if(rf.open(QIODevice::ReadOnly))
          {
            refBytes = rf.readAll();
            rf.close();
          }
          QFile::remove(ref);
          if(refBytes.isEmpty())
            return {Verdict::Skipped,
                    "gst-launch could not dump a videotestsrc reference"};

          const auto f
              = drive(plug, ctx, factory, p.pipeline, {3, 8000, 0, 0, true});
          if(!f.connected || !f.hasVideo || f.raw.empty())
            return {Verdict::Skipped,
                    "connected=" + std::to_string(f.connected)
                        + " video=" + std::to_string(f.hasVideo)
                        + " RGBA frames=" + std::to_string(f.raw.size())};

          const std::size_t frameBytes = std::size_t(f.width) * f.height * 4;
          if(frameBytes == 0 || std::size_t(refBytes.size()) < frameBytes)
            return {Verdict::Skipped,
                    "the reference dump is " + std::to_string(refBytes.size())
                        + " bytes for a " + std::to_string(f.width) + "x"
                        + std::to_string(f.height) + " RGBA frame"};

          const int refFrames = int(std::size_t(refBytes.size()) / frameBytes);
          bool exact = false;
          int bestDiff = 1 << 30;
          for(const auto& got : f.raw)
          {
            if(got.size() != frameBytes)
              continue;
            for(int j = 0; j < refFrames && !exact; j++)
            {
              const auto* want
                  = reinterpret_cast<const uint8_t*>(refBytes.constData())
                    + std::size_t(j) * frameBytes;
              int worst = 0;
              for(std::size_t i = 0; i < frameBytes; i++)
                worst = std::max(worst, std::abs(int(got[i]) - int(want[i])));
              bestDiff = std::min(bestDiff, worst);
              if(worst == 0)
                exact = true;
            }
          }
          const bool wantsAudio = p.label.contains(QStringLiteral("tone"));
          if(!exact)
            return {Verdict::Skipped,
                    "closest videotestsrc reference frame differs by "
                        + std::to_string(bestDiff)};
          if(wantsAudio && !f.hasAudio)
            return {Verdict::Skipped, "no /audio node was published"};
          return {Verdict::Works,
                  "byte-for-byte equal to a gst-launch videotestsrc reference at "
                      + std::to_string(f.width) + "x" + std::to_string(f.height)
                      + (wantsAudio
                             ? "; /audio published (its samples reach the "
                               "parameter through the audio engine tick, which "
                               "this harness does not run)"
                             : "")};
        });
        note(p, out.verdict, out.detail);
        continue;
      }

      // -------------------------------------------------------------------
      // Input: a network receiver, fed by its own shipped sender where that
      // sender runs here, and by a substitute peer otherwise.
      if(auto pair = pairs.find(p.label); pair != pairs.end() && isVideoInput)
      {
        const QString senderLabel = pair->second;
        const QString senderPipeline
            = byLabel.count(senderLabel) ? byLabel.at(senderLabel) : QString{};

        if(p.label.startsWith(QStringLiteral("ST 2110"))
           || p.label.startsWith(QStringLiteral("ST 2022")))
        {
          note(p, Verdict::Skipped,
               "multicast on 239.0.0.1: this harness will not put traffic on "
               "the host's real network, and loopback carries no multicast "
               "route");
          continue;
        }

        const std::string senderFailure
            = senderPipeline.isEmpty()
                  ? std::string{"no shipped sender for this receiver"}
                  : parseFailure(senderPipeline);

        const char* substitute
            = p.label.startsWith(QStringLiteral("UDP MJPEG")) ? "jpegenc"
                                                              : h264Encoder();
        if(!senderFailure.empty() && !substitute)
        {
          note(p, Verdict::Skipped,
               "its shipped sender does not run here (" + senderFailure
                   + ") and there is no substitute encoder");
          continue;
        }

        const int port = p.label == "RIST receiver" ? freeEvenUdpPort()
                                                    : freeUdpPort();
        if(port <= 0)
        {
          note(p, Verdict::Skipped, "could not reserve a UDP port");
          continue;
        }

        QString receiver = p.pipeline;
        QString sender;
        int settle = 900;
        if(p.label.startsWith(QStringLiteral("UDP")))
        {
          receiver.replace(
              QStringLiteral("udpsrc port=5000"),
              QStringLiteral("udpsrc port=%1").arg(port));
          sender = senderFailure.empty()
                       ? senderFromOutputPreset(m, rawMaster, senderPipeline)
                             .replace(
                                 QStringLiteral("port=5000"),
                                 QStringLiteral("port=%1").arg(port))
                       : senderFromOutputPreset(
                             m, rawMaster,
                             QStringLiteral("appsrc name=video ! videoconvert ! %1 "
                                            "! h264parse ! rtph264pay "
                                            "config-interval=1 pt=96 ! udpsink "
                                            "host=127.0.0.1 port=%2")
                                 .arg(QString::fromUtf8(substitute))
                                 .arg(port));
        }
        else if(p.label == "SRT receiver")
        {
          receiver.replace(
              QStringLiteral("srt://127.0.0.1:4200"),
              QStringLiteral("srt://127.0.0.1:%1").arg(port));
          sender = senderFailure.empty()
                       ? senderFromOutputPreset(m, rawMaster, senderPipeline)
                             .replace(
                                 QStringLiteral("srt://:4200"),
                                 QStringLiteral("srt://:%1").arg(port))
                       : senderFromOutputPreset(
                             m, rawMaster,
                             QStringLiteral("appsrc name=video ! videoconvert ! %1 "
                                            "! h264parse ! mpegtsmux ! srtsink "
                                            "uri=srt://:%2?mode=listener")
                                 .arg(QString::fromUtf8(substitute))
                                 .arg(port));
          settle = 1800;
        }
        else if(p.label == "RIST receiver")
        {
          receiver.replace(
              QStringLiteral("port=5004"), QStringLiteral("port=%1").arg(port));
          sender = senderFailure.empty()
                       ? senderFromOutputPreset(m, rawMaster, senderPipeline)
                             .replace(
                                 QStringLiteral("port=5004"),
                                 QStringLiteral("port=%1").arg(port))
                       : senderFromOutputPreset(
                             m, rawMaster,
                             QStringLiteral("appsrc name=video ! videoconvert ! %1 "
                                            "! h264parse ! rtph264pay "
                                            "config-interval=1 pt=96 ! ristsink "
                                            "address=127.0.0.1 port=%2")
                                 .arg(QString::fromUtf8(substitute))
                                 .arg(port));
          settle = 1800;
        }
        else
        {
          note(p, Verdict::Skipped,
               senderFailure.empty()
                   ? std::string{"WHIP/WHEP needs a signalling server, and this "
                                 "harness has none; gst-plugins-rs's webrtchttp "
                                 "elements are what carry it"}
                   : "its shipped sender does not run here: " + senderFailure);
          continue;
        }

        const bool usedShippedSender = senderFailure.empty();
        const auto out
            = runBounded(90, QStringLiteral("pair"), [&]() -> Outcome {
                Peer peer{sender};
                if(!peer.started)
                  return {Verdict::Skipped, "gst-launch-1.0 would not start"};
                QThread::msleep(settle);
                const auto f = drive(
                    plug, ctx, factory, receiver, {6, 20000, m.width, m.height});
                std::string why;
                const auto best = bestOf(f, m, block, kBlockMargin);
                if(f.connected && f.hasVideo
                   && carriesThePicture(best, kToleranceLossy, why))
                  return {Verdict::Works,
                          "received " + std::to_string(f.decoded) + " frames from "
                              + (usedShippedSender
                                     ? "its own shipped sender preset"
                                     : "a substitute sender (the shipped one "
                                       "needs an absent encoder)")
                              + "; matched master frame "
                              + std::to_string(best.index) + " to within "
                              + std::to_string(best.maxDev)};
                return {Verdict::Skipped,
                        "connected=" + std::to_string(f.connected)
                            + " video=" + std::to_string(f.hasVideo)
                            + " frames=" + std::to_string(f.decoded) + " " + why
                            + peer.diagnostics()};
              });
        note(p, out.verdict, out.detail);

        if(usedShippedSender && out.verdict == Verdict::Works)
        {
          provenSenderLabel = senderLabel;
          provenSenderDetail
              = "its own receiver preset decoded the master picture out of what "
                "it sent (" + out.detail + ")";
        }
        continue;
      }

      // -------------------------------------------------------------------
      // Input: live sources with no oracle here.
      if(p.label == "Screen capture (X11)" || p.label == "Webcam (V4L2)")
      {
        const bool present = p.label.startsWith(QStringLiteral("Screen"))
                                 ? !qgetenv("DISPLAY").isEmpty()
                                 : QFile::exists(QStringLiteral("/dev/video0"));
        if(!present)
        {
          note(p, Verdict::Skipped,
               p.label.startsWith(QStringLiteral("Screen"))
                   ? "no DISPLAY on this host"
                   : "no /dev/video0 on this host");
          continue;
        }
        const auto out = runBounded(60, QStringLiteral("live"), [&]() -> Outcome {
          const auto f = drive(plug, ctx, factory, p.pipeline, {3, 15000, 0, 0});
          const bool live = f.connected && f.hasVideo && f.decoded > 0;
          if(!live)
            return {Verdict::Skipped,
                    "connected=" + std::to_string(f.connected)
                        + " video=" + std::to_string(f.hasVideo)
                        + " frames=" + std::to_string(f.decoded)};
          return {Verdict::Built,
                  "captured " + std::to_string(f.decoded) + " frames at "
                      + std::to_string(f.width) + "x" + std::to_string(f.height)
                      + "; a live screen or camera has no known picture to "
                        "compare against"};
        });
        note(p, out.verdict, out.detail);
        continue;
      }

      if(p.label == "RTSP stream")
      {
        note(p, Verdict::Skipped,
             "no RTSP server on this host to serve rtsp:// (gst-rtsp-server's "
             "test-launch is not installed)");
        continue;
      }
      if(p.label.contains(QStringLiteral("Decklink")))
      {
        note(p, Verdict::Skipped,
             "the decklink elements exist but this host has no Blackmagic card");
        continue;
      }
      if(p.label.contains(QStringLiteral("JACK")))
      {
        note(p, Verdict::Skipped,
             "no JACK server is running on this host (jackdbus is present but "
             "idle)");
        continue;
      }
      if(p.label.startsWith(QStringLiteral("ST 2110"))
         || p.label.startsWith(QStringLiteral("ST 2022"))
         || p.label.startsWith(QStringLiteral("SMPTE 302M"))
         || p.label.startsWith(QStringLiteral("RTP multicast")))
      {
        note(p, Verdict::Skipped,
             "multicast on 239.0.0.1: this harness will not put traffic on the "
             "host's real network, and loopback carries no multicast route");
        continue;
      }

      // -------------------------------------------------------------------
      // Input: audio-only.
      if(isInput && !isVideoInput)
      {
        const auto out = runBounded(45, QStringLiteral("audioin"), [&]() -> Outcome {
          const auto f = drive(plug, ctx, factory, p.pipeline, {0, 0, 0, 0});
          if(!(f.connected && f.hasAudio))
            return {Verdict::Skipped,
                    "connected=" + std::to_string(f.connected)
                        + " audio=" + std::to_string(f.hasAudio)};
          return {Verdict::Built,
                  "connected and published /audio; the samples reach the "
                  "parameter through the audio engine tick, which this harness "
                  "does not run"};
        });
        note(p, out.verdict, out.detail);
        continue;
      }

      // -------------------------------------------------------------------
      // Outputs.
      if(!isInput)
      {
        // Recorders and the shared-memory sink: run the preset for real and
        // read back what it produced.
        const bool records = p.pipeline.contains(QStringLiteral("filesink"));
        const bool shm = p.pipeline.contains(QStringLiteral("shmsink"));
        const bool tcp = p.pipeline.contains(QStringLiteral("tcpserversink"));

        if(records)
        {
          static const QRegularExpression locRe{QStringLiteral("location=(\\S+)")};
          const auto shipped_loc = locRe.match(p.pipeline);
          const QString suffix
              = shipped_loc.hasMatch()
                    ? QFileInfo{shipped_loc.captured(1)}.suffix()
                    : QStringLiteral("bin");
          const QString target
              = scratch + QStringLiteral("/rec-%1.%2").arg(only).arg(suffix);
          QString pipeline = p.pipeline;
          // Every recording preset writes into /tmp; point it at the scratch
          // directory so the sweep cannot collide with a real recording.
          static const QRegularExpression loc{QStringLiteral("location=[^ ]+")};
          pipeline.replace(loc, QStringLiteral("location=") + target);
          QFile::remove(target);

          const auto out
              = runBounded(120, QStringLiteral("rec"), [&]() -> Outcome {
                  {
                    Peer peer{senderFromOutputPreset(m, rawMaster, pipeline)};
                    if(!peer.started)
                      return {Verdict::Skipped, "gst-launch-1.0 would not start"};
                    QThread::msleep(4000);
                    peer.interrupt();
                    peer.proc.waitForFinished(20000);
                  }
                  if(!QFile::exists(target) || QFileInfo{target}.size() == 0)
                    return {Verdict::Skipped,
                            "the preset produced no file at "
                                + target.toStdString()};
                  const auto frames = decodeFile(target.toStdString(), m);
                  const auto best = bestOfDecoded(frames, m, block, kBlockMargin);
                  std::string why;
                  if(carriesThePicture(best, kToleranceLossy, why))
                    return {Verdict::Works,
                            "wrote " + std::to_string(QFileInfo{target}.size())
                                + " bytes; decoded it back and matched master "
                                  "frame "
                                + std::to_string(best.index) + " to within "
                                + std::to_string(best.maxDev)};
                  return {Verdict::Skipped,
                          "the file it wrote did not decode back to the master: "
                              + why};
                });
          note(p, out.verdict, out.detail);
          continue;
        }

        if(shm)
        {
          const QString sock = scratch + QStringLiteral("/shm.sock");
          QFile::remove(sock);
          QString pipeline = p.pipeline;
          pipeline.replace(
              QStringLiteral("socket-path=/tmp/score_video"),
              QStringLiteral("socket-path=") + sock);
          const QString dump = scratch + QStringLiteral("/shm.rgba");
          QFile::remove(dump);

          const auto out
              = runBounded(90, QStringLiteral("shm"), [&]() -> Outcome {
                  Peer sender{senderFromOutputPreset(m, rawMaster, pipeline)};
                  if(!sender.started)
                    return {Verdict::Skipped, "gst-launch-1.0 would not start"};
                  QThread::msleep(1500);
                  QProcess rd;
                  rd.start(
                      QStringLiteral("gst-launch-1.0"),
                      gstArgs(
                          QStringLiteral(
                              "shmsrc socket-path=%1 is-live=true ! "
                              "video/x-raw,format=RGBA,width=%2,height=%3,"
                              "framerate=25/1 ! videoconvert ! "
                              "video/x-raw,format=RGBA ! filesink location=%4")
                              .arg(sock)
                              .arg(m.width)
                              .arg(m.height)
                              .arg(dump)));
                  if(!rd.waitForStarted(5000))
                    return {Verdict::Skipped, "the shmsrc reader would not start"};
                  QThread::msleep(3000);
                  rd.terminate();
                  rd.waitForFinished(8000);
                  rd.kill();

                  QFile f{dump};
                  if(!f.open(QIODevice::ReadOnly))
                    return {Verdict::Skipped, "the shmsrc reader wrote nothing"};
                  const QByteArray bytes = f.readAll();
                  f.close();
                  const std::size_t frameBytes = std::size_t(m.width) * m.height * 4;
                  if(std::size_t(bytes.size()) < frameBytes)
                    return {Verdict::Skipped,
                            "the shmsrc reader wrote only "
                                + std::to_string(bytes.size()) + " bytes"};
                  // The shared segment carries RGBA, which is what the master
                  // is: compare the first whole frame against it directly.
                  AVFrame fr{};
                  fr.format = AV_PIX_FMT_RGBA;
                  fr.width = m.width;
                  fr.height = m.height;
                  fr.data[0] = reinterpret_cast<uint8_t*>(
                      const_cast<char*>(bytes.constData()));
                  fr.linesize[0] = m.width * 4;
                  const auto rgb = toRgb24(fr, m.width, m.height);
                  const auto best = matchOne(rgb, m, block, kBlockMargin);
                  std::string why;
                  if(carriesThePicture(best, kToleranceYuvExact, why))
                    return {Verdict::Works,
                            "the shared segment carried master frame "
                                + std::to_string(best.index) + " to within "
                                + std::to_string(best.maxDev)};
                  return {Verdict::Skipped,
                          "the shared segment did not carry the master: " + why};
                });
          note(p, out.verdict, out.detail);
          continue;
        }

        if(tcp)
        {
          const int port = freeUdpPort();
          QString pipeline = p.pipeline;
          pipeline.replace(
              QStringLiteral("port=5000"), QStringLiteral("port=%1").arg(port));
          const QString dump = scratch + QStringLiteral("/tcp.rgba");
          QFile::remove(dump);

          const auto out
              = runBounded(90, QStringLiteral("tcp"), [&]() -> Outcome {
                  Peer sender{senderFromOutputPreset(m, rawMaster, pipeline)};
                  if(!sender.started)
                    return {Verdict::Skipped, "gst-launch-1.0 would not start"};
                  QThread::msleep(1200);
                  QProcess rd;
                  rd.start(
                      QStringLiteral("gst-launch-1.0"),
                      gstArgs(
                          QStringLiteral(
                              "tcpclientsrc host=127.0.0.1 port=%1 ! jpegdec ! "
                              "videoconvert ! video/x-raw,format=RGBA ! filesink "
                              "location=%2")
                              .arg(port)
                              .arg(dump),
                          false));
                  if(!rd.waitForStarted(5000))
                    return {Verdict::Skipped, "the TCP reader would not start"};
                  QThread::msleep(3000);
                  rd.terminate();
                  rd.waitForFinished(10000);
                  rd.kill();
                  QFile df{dump};
                  if(!df.open(QIODevice::ReadOnly))
                    return {Verdict::Skipped, "the TCP reader received nothing"};
                  const QByteArray bytes = df.readAll();
                  df.close();
                  const std::size_t frameBytes
                      = std::size_t(m.width) * m.height * 4;
                  if(std::size_t(bytes.size()) < frameBytes)
                    return {Verdict::Skipped,
                            "the TCP reader received only "
                                + std::to_string(bytes.size()) + " bytes"};
                  AVFrame fr{};
                  fr.format = AV_PIX_FMT_RGBA;
                  fr.width = m.width;
                  fr.height = m.height;
                  fr.data[0] = reinterpret_cast<uint8_t*>(
                      const_cast<char*>(bytes.constData()));
                  fr.linesize[0] = m.width * 4;
                  const auto rgb = toRgb24(fr, m.width, m.height);
                  const auto best = matchOne(rgb, m, block, kBlockMargin);
                  std::string why;
                  if(carriesThePicture(best, kToleranceLossy, why))
                    return {Verdict::Works,
                            "a TCP client decoded master frame "
                                + std::to_string(best.index)
                                + " out of the stream, to within "
                                + std::to_string(best.maxDev)};
                  return {Verdict::Skipped,
                          "what came off the socket did not decode back to the "
                          "master: " + why + sender.diagnostics()};
                });
          note(p, out.verdict, out.detail);
          continue;
        }

        if(p.label.contains(QStringLiteral("V4L2 loopback")))
        {
          note(p, Verdict::Skipped,
               QFile::exists(QStringLiteral("/dev/video10"))
                   ? "a /dev/video10 exists but is not a provisioned "
                     "v4l2loopback sink for this harness"
                   : "no /dev/video10: the v4l2loopback kernel module is not "
                     "loaded, and loading it needs root");
          continue;
        }

        if(p.pipeline.contains(QStringLiteral("autovideosink")))
        {
          const auto out
              = runBounded(60, QStringLiteral("disp"), [&]() -> Outcome {
                  Peer sender{senderFromOutputPreset(m, rawMaster, p.pipeline)};
                  if(!sender.started)
                    return {Verdict::Skipped, "gst-launch-1.0 would not start"};
                  QThread::msleep(2500);
                  const bool alive = sender.proc.state() == QProcess::Running;
                  return {alive ? Verdict::Built : Verdict::Skipped,
                          alive ? "the preset ran and kept running with a real "
                                  "display attached; what a window shows has no "
                                  "readback here"
                                : "the pipeline exited immediately"};
                });
          note(p, out.verdict, out.detail);
          continue;
        }

        // Audio sinks and everything else with no readback.
        const auto out = runBounded(60, QStringLiteral("out"), [&]() -> Outcome {
          Peer sender{senderFromOutputPreset(m, rawMaster, p.pipeline)};
          if(!sender.started)
            return {Verdict::Skipped, "gst-launch-1.0 would not start"};
          QThread::msleep(2500);
          const bool alive = sender.proc.state() == QProcess::Running;
          return {alive ? Verdict::Built : Verdict::Skipped,
                  alive ? "the preset ran for real and stayed running; this "
                          "harness has no readback for what it sent"
                        : "the pipeline exited immediately"};
        });
        note(p, out.verdict, out.detail);
        continue;
      }

      note(p, Verdict::Built,
           "every element exists and gst_parse_launch built it; no local source "
           "to drive this input");
    }

    ran = true;
  });

  if(!ran)
  {
    const std::string why
        = skipReason.empty() ? std::string{"the harness could not run"} : skipReason;
    SKIP(why);
  }

  REQUIRE(reports.size() == 1);
  const auto& r = reports.front();

  std::string payload;
  payload += toChar(r.verdict);
  payload += r.detail;
  if(!provenSenderLabel.isEmpty())
  {
    payload += kProvenSep;
    payload += provenSenderLabel.toStdString();
    payload += kDetailSep;
    payload += provenSenderDetail;
  }

  const QString out = QString::fromLocal8Bit(qgetenv(kOutEnv));
  REQUIRE_FALSE(out.isEmpty());
  QFile f{out};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(payload.data(), qsizetype(payload.size()));
  f.close();
}

// ---------------------------------------------------------------------------

TEST_CASE("every shipped GStreamer preset is accounted for",
          "[gfx][gstreamer][presets][media]")
{
  std::vector<Report> reports;
  std::string skipReason;
  bool ran = false;

  run_in_app([&](const score::GUIApplicationContext& appctx) {
    auto* doc = new_document(appctx);
    if(!doc)
    {
      skipReason = "no document delegate";
      return;
    }
    const auto& ctx = doc->context();
    GStreamer::ProtocolFactory factory;

    if(!gstreamerUsable())
    {
      skipReason = "the GStreamer libraries could not be loaded";
      return;
    }

    std::vector<Preset> shipped;
    {
      auto enums = factory.getEnumerators(ctx);
      for(auto& [group, e] : enums)
      {
        e->enumerate(
            [&, g = group](const QString& label, const Device::DeviceSettings& s) {
              const auto gs
                  = s.deviceSpecificSettings.value<GStreamer::GStreamerSettings>();
              shipped.push_back({g, label, s.name, gs.pipeline});
            });
        delete e;
      }
    }
    REQUIRE(shipped.size() >= 30);

    const QString self = QCoreApplication::applicationFilePath();
    const QString scratch
        = QDir::tempPath() + QStringLiteral("/score-gst-presets");
    QDir{}.mkpath(scratch);

    std::map<QString, Report> provenSenders;

    for(int i = 0; i < int(shipped.size()); i++)
    {
      const QString out
          = scratch + QStringLiteral("/verdict-%1.bin").arg(i);
      QFile::remove(out);

      auto env = QProcessEnvironment::systemEnvironment();
      env.insert(QString::fromUtf8(kIndexEnv), QString::number(i));
      env.insert(QString::fromUtf8(kOutEnv), out);
      // GStreamer's parse errors end up in the report, so they have to read the
      // same on every host.
      env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
      env.insert(QStringLiteral("LANG"), QStringLiteral("C"));

      QProcess probe;
      probe.setProcessEnvironment(env);
      probe.setProcessChannelMode(QProcess::ForwardedErrorChannel);
#if !defined(_WIN32)
      // Its own session, so a peer it spawned cannot outlive the kill.
      probe.setChildProcessModifier([] { ::setsid(); });
#endif
      probe.start(self, {QString::fromUtf8(kProbeCase)});

      Report r{shipped[i], Verdict::Wedged, {}};
      if(!probe.waitForStarted(10000))
      {
        r.detail = "the probe process would not start";
      }
      else if(!probe.waitForFinished(kPresetBudgetMs))
      {
#if !defined(_WIN32)
        ::kill(-probe.processId(), SIGKILL);
#endif
        probe.kill();
        probe.waitForFinished(5000);
        r.detail = "did not return within "
                   + std::to_string(kPresetBudgetMs / 1000)
                   + "s and had to be killed";
      }
      else
      {
        QFile f{out};
        if(f.open(QIODevice::ReadOnly))
        {
          const QByteArray raw = f.readAll();
          f.close();
          if(!raw.isEmpty())
          {
            const int sep = raw.indexOf(kProvenSep);
            const QByteArray head = sep < 0 ? raw : raw.left(sep);
            r.verdict = fromChar(head[0]);
            r.detail = std::string{head.constData() + 1,
                                   std::size_t(head.size() - 1)};
            if(sep >= 0)
            {
              const QByteArray tail = raw.mid(sep + 1);
              const int d = tail.indexOf(kDetailSep);
              if(d > 0)
              {
                const QString label = QString::fromUtf8(tail.left(d));
                const std::string detail
                    = tail.mid(d + 1).toStdString();
                for(const auto& q : shipped)
                  if(q.label == label)
                    provenSenders[label] = {q, Verdict::Works, detail};
              }
            }
          }
          else
          {
            r.detail = "the probe produced an empty verdict";
          }
        }
        else
        {
          r.detail = "the probe exited " + std::to_string(probe.exitCode())
                     + " without writing a verdict";
        }
      }
      QFile::remove(out);

      std::fprintf(
          stderr, "[preset] %s  %s / %s  %s\n", toString(r.verdict),
          r.preset.group.toStdString().c_str(),
          r.preset.label.toStdString().c_str(), r.detail.c_str());
      std::fflush(stderr);
      reports.push_back(std::move(r));
    }

    // A sender proved by its own receiver gets that verdict rather than the
    // weaker one its own row earned.
    for(auto& r : reports)
      if(auto it = provenSenders.find(r.preset.label); it != provenSenders.end())
        r = it->second;

    ran = true;
  });

  if(!ran)
  {
    const std::string why
        = skipReason.empty() ? std::string{"the harness could not run"} : skipReason;
    SKIP(why);
  }

  INFO(render(reports));

  CHECK(reports.size() >= 30);

  // A preset whose description does not BUILD is a defect in the preset, not a
  // property of the host -- with one exception: GST_PARSE_ERROR_NO_SUCH_ELEMENT
  // means the host lacks a plugin. The error CODE is checked rather than the
  // message, which is translated.
  for(const auto& r : reports)
  {
    if(r.verdict != Verdict::Skipped)
      continue;
    const auto tag = r.detail.find("[gst-parse-error ");
    if(tag == std::string::npos)
      continue;
    const int code = std::atoi(r.detail.c_str() + tag + 17);
    INFO(r.preset.label.toStdString() << ": " << r.detail);
    CHECK(code == kParseNoSuchElement);
  }

  // Nothing may wedge: a preset that has to be killed is a defect in what it
  // opens or in how the device closes it.
  for(const auto& r : reports)
  {
    INFO(r.preset.label.toStdString() << ": " << r.detail);
    CHECK(r.verdict != Verdict::Wedged);
  }

  // ...and enough of them really did carry a picture that an all-built report
  // cannot pass for coverage.
  int works = 0;
  for(const auto& r : reports)
    if(r.verdict == Verdict::Works)
      works++;
  INFO(render(reports));
  CHECK(works >= 5);
}

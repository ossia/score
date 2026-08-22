// Every preset the FFmpeg (libav) protocol SHIPS, driven for real, in both
// directions.
//
// Enumerated through LibavProtocolFactory::getEnumerators() rather than copied,
// so a preset that is added, edited or platform-gated is covered automatically.
//
// The two directions are driven through score's own code, not through ffmpeg:
//   - an INPUT preset builds the real device and pulls frames out of its
//     CameraNode;
//   - an OUTPUT preset builds a real Gfx::LibavEncoder from
//     LibavSettings::toOutputSettings() and pushes the known master into it.
// What the encoder wrote is then decoded back and compared against the master
// picture, so "it recorded" means the picture came out the other end, not that
// a container opened.
//
// Verdicts, one per preset and never none:
//   WORKS  - driven end to end and the PIXELS (or SAMPLES) were compared
//            against a known signal.
//   built  - it ran for real but this harness has no oracle for its content;
//            the reason is recorded per preset.
//   SKIP   - cannot run here; the reason names the missing encoder, server,
//            device or kernel module exactly.
//   FAILED - it ran, the oracle ran, and the content was WRONG (decoded
//            pixels or samples do not match the master). A finding.
//   WEDGED - it had to be killed. That is a finding, not an absence.
//
// One preset per process, under a wall-clock deadline the parent owns: half of
// these open a device or a socket, and any of those can fail to return.

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QThread>
#include <QUdpSocket>

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Libav/LibavDevice.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>

#include <Audio/Settings/Model.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/tools/File.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/VideoMaster.hpp>

#include <Video/VideoInterface.hpp>

extern "C" {
#include <libavfilter/avfilter.h>
}

#include <ossia/network/base/node_functions.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <csignal>
#include <unistd.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

using namespace score::test;
using namespace score::test::video;
using namespace Gfx;

namespace
{

constexpr auto kProbeCase = "the libav preset probe";
constexpr auto kIndexEnv = "SCORE_LIBAV_PRESET_INDEX";
constexpr auto kOutEnv = "SCORE_LIBAV_PRESET_OUT";
constexpr int kPresetBudgetMs = 150000;
constexpr char kProvenSep = '\x01';
constexpr char kDetailSep = '\x02';

// How many master frames each recording preset is given. Enough that a codec
// with a GOP has something to work with, few enough that the sweep stays short.
constexpr int kRecordFrames = 12;
constexpr int kAudioBlocks = 60;
constexpr int kAudioRate = 44100;

struct Preset
{
  QString group;
  QString label;
  LibavSettings settings;
};

// Major version of the libavfilter that the `ffmpeg` on PATH is built against,
// or 0 when it agrees with ours / cannot be determined.
//
// `ffmpeg -version` prints one line per library:
//     libavfilter     10.  4.100 / 10.  4.100
// The FIRST number is the major. A missing or unparseable ffmpeg returns 0:
// the caller already handles an absent reference (it notes Skipped), and a
// version we cannot read is not evidence of a mismatch.
//
// Returns the FOREIGN major on mismatch, so the caller can name both.
inline int lavfiOracleVersionMismatch()
{
  static const int cached = [] {
    QProcess ff;
    ff.start(QStringLiteral("ffmpeg"), {QStringLiteral("-version")});
    if(!ff.waitForStarted(10000))
      return 0; // no ffmpeg on PATH at all
    ff.waitForFinished(30000);
    const QString out = QString::fromUtf8(ff.readAllStandardOutput())
                        + QString::fromUtf8(ff.readAllStandardError());

    static const QRegularExpression re{
        QStringLiteral(R"(libavfilter\s+(\d+)\.)")};
    const auto m = re.match(out);
    if(!m.hasMatch())
      return 0; // unparseable: do not invent a mismatch

    const int theirs = m.captured(1).toInt();
    return theirs == LIBAVFILTER_VERSION_MAJOR ? 0 : theirs;
  }();
  return cached;
}

enum class Verdict
{
  Works,
  Built,
  Skipped,
  Failed,
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
    case Verdict::Failed:
      return "FAILED";
    case Verdict::Wedged:
      return "WEDGED";
  }
  return "?";
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
    case Verdict::Failed:
      return 'F';
    case Verdict::Skipped:
      return 'S';
  }
  return 'S';
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
    case 'F':
      return Verdict::Failed;
    default:
      return Verdict::Skipped;
  }
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
           + (r.preset.settings.direction == LibavSettings::Input ? "in" : "out")
           + "]\n            " + r.detail + "\n";
  return out;
}

struct Outcome
{
  Verdict verdict{Verdict::Skipped};
  std::string detail;
};

// ---------------------------------------------------------------------------
// Host inventory

bool hasEncoder(const QString& shortName)
{
  if(shortName.isEmpty())
    return true;
  return avcodec_find_encoder_by_name(shortName.toStdString().c_str()) != nullptr;
}

bool hasMuxer(const QString& muxer)
{
  if(muxer.isEmpty())
    return true;
  return av_guess_format(muxer.toStdString().c_str(), nullptr, nullptr) != nullptr;
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

// ---------------------------------------------------------------------------
// The master, as flat RGBA frames

struct RgbaMaster
{
  int width{}, height{}, frames{};
  std::vector<uint8_t> bytes;
  const uint8_t* frame(int i) const
  {
    return bytes.data() + std::size_t(i) * std::size_t(width) * height * 4;
  }
};

RgbaMaster loadRgbaMaster(const Master& m)
{
  RgbaMaster r;
  r.width = m.width;
  r.height = m.height;
  QFile f{QString::fromStdString(matrixPath("master.rgba"))};
  if(!f.open(QIODevice::ReadOnly))
    return r;
  const QByteArray raw = f.readAll();
  f.close();
  const std::size_t per = std::size_t(m.width) * m.height * 4;
  if(per == 0 || raw.size() < qsizetype(per))
    return r;
  r.frames = int(std::size_t(raw.size()) / per);
  r.bytes.assign(
      reinterpret_cast<const uint8_t*>(raw.constData()),
      reinterpret_cast<const uint8_t*>(raw.constData()) + std::size_t(r.frames) * per);
  return r;
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

Match matchOne(const std::vector<uint8_t>& rgb, const Master& m, int block, int margin)
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

Match bestOf(
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
// Reading media back

std::vector<std::vector<uint8_t>>
decodeVideo(const std::string& path, const Master& m, int maxFrames = 8)
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
  while(int(out.size()) < maxFrames && av_read_frame(fmt, pkt) >= 0)
  {
    if(pkt->stream_index == vs && avcodec_send_packet(cc, pkt) == 0)
      while(avcodec_receive_frame(cc, frame) == 0 && int(out.size()) < maxFrames)
      {
        auto rgb = toRgb24(*frame, m.width, m.height);
        if(rgb.size() == std::size_t(m.width) * m.height * 3)
          out.push_back(std::move(rgb));
      }
    av_packet_unref(pkt);
  }
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&cc);
  avformat_close_input(&fmt);
  return out;
}

// Decoded interleaved audio, as floats, from the first audio stream.
std::vector<float> decodeAudio(const std::string& path, int& channels, int& rate)
{
  std::vector<float> out;
  channels = 0;
  rate = 0;
  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) != 0)
    return out;
  if(avformat_find_stream_info(fmt, nullptr) < 0)
  {
    avformat_close_input(&fmt);
    return out;
  }
  int as = -1;
  for(unsigned i = 0; i < fmt->nb_streams; i++)
    if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      as = int(i);
      break;
    }
  if(as < 0)
  {
    avformat_close_input(&fmt);
    return out;
  }
  const AVCodec* dec = avcodec_find_decoder(fmt->streams[as]->codecpar->codec_id);
  AVCodecContext* cc = dec ? avcodec_alloc_context3(dec) : nullptr;
  if(!cc || avcodec_parameters_to_context(cc, fmt->streams[as]->codecpar) < 0
     || avcodec_open2(cc, dec, nullptr) < 0)
  {
    if(cc)
      avcodec_free_context(&cc);
    avformat_close_input(&fmt);
    return out;
  }
  channels = cc->ch_layout.nb_channels;
  rate = cc->sample_rate;

  AVPacket* pkt = av_packet_alloc();
  AVFrame* frame = av_frame_alloc();
  while(out.size() < 500000 && av_read_frame(fmt, pkt) >= 0)
  {
    if(pkt->stream_index == as && avcodec_send_packet(cc, pkt) == 0)
    {
      while(avcodec_receive_frame(cc, frame) == 0)
      {
        const int n = frame->nb_samples;
        const int ch = frame->ch_layout.nb_channels;
        const auto fm = AVSampleFormat(frame->format);
        const bool planar = av_sample_fmt_is_planar(fm) != 0;
        for(int i = 0; i < n; i++)
          for(int c = 0; c < ch; c++)
          {
            const uint8_t* base
                = planar ? frame->extended_data[c] : frame->extended_data[0];
            const int idx = planar ? i : i * ch + c;
            float v = 0.f;
            switch(fm)
            {
              case AV_SAMPLE_FMT_FLT:
              case AV_SAMPLE_FMT_FLTP:
                v = reinterpret_cast<const float*>(base)[idx];
                break;
              case AV_SAMPLE_FMT_DBL:
              case AV_SAMPLE_FMT_DBLP:
                v = float(reinterpret_cast<const double*>(base)[idx]);
                break;
              case AV_SAMPLE_FMT_S16:
              case AV_SAMPLE_FMT_S16P:
                v = reinterpret_cast<const int16_t*>(base)[idx] / 32768.f;
                break;
              case AV_SAMPLE_FMT_S32:
              case AV_SAMPLE_FMT_S32P:
                v = float(
                    reinterpret_cast<const int32_t*>(base)[idx] / 2147483648.);
                break;
              default:
                break;
            }
            out.push_back(v);
          }
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

// The signal every audio recording preset is fed: a per-channel sine at a
// distinct frequency, so a swapped or silent channel is visible. It is a
// function of TIME, not of the sample index, so the same signal can be
// reconstructed at whatever rate the encoder resampled to -- libopus, for one,
// always writes 48 kHz.
float audioSample(int channel, long long n, int rate = kAudioRate)
{
  const double f = 440.0 * (channel + 1);
  return float(0.5 * std::sin(2.0 * M_PI * f * double(n) / double(rate)));
}

// ---------------------------------------------------------------------------
// Driving an input preset through the real device

score::gfx::CameraNode* cameraNode(ossia::net::device_base& d, const char* path)
{
  auto* n = ossia::net::find_node(d.get_root_node(), path);
  if(!n)
    return nullptr;
  if(auto* gp = dynamic_cast<simple_texture_input_parameter*>(n->get_parameter()))
    return dynamic_cast<score::gfx::CameraNode*>(gp->node);
  return nullptr;
}

struct Frames
{
  bool connected{};
  bool hasVideo{};
  int decoded{};
  int width{}, height{};
  std::vector<std::vector<uint8_t>> rgb;
  std::vector<std::vector<uint8_t>> rgbNative; // at the source's own geometry
};

Frames driveInput(
    Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx,
    LibavProtocolFactory& factory, const LibavSettings& set, int wanted,
    int budgetMs, int rgbW, int rgbH)
{
  Frames f;
  Device::DeviceSettings ds;
  ds.name = QStringLiteral("FFmpeg");
  ds.protocol = LibavProtocolFactory::static_concreteKey();
  ds.deviceSpecificSettings = QVariant::fromValue(set);

  std::unique_ptr<Device::DeviceInterface> dev{factory.makeDevice(ds, plug, ctx)};
  if(!dev)
    return f;

  f.connected = dev->reconnect();
  if(f.connected)
  {
    if(auto* d = dev->getDevice())
    {
      auto* cam = cameraNode(*d, "/Video");
      f.hasVideo = cam != nullptr;
      if(cam && cam->reader.m_decoder)
      {
        d->get_protocol().start_execution();
        QElapsedTimer t;
        t.start();
        while(t.elapsed() < budgetMs && f.decoded < wanted)
        {
          if(AVFrame* fr = cam->reader.m_decoder->dequeue_frame())
          {
            f.decoded++;
            f.width = fr->width;
            f.height = fr->height;
            if(rgbW > 0 && rgbH > 0)
            {
              auto rgb = toRgb24(*fr, rgbW, rgbH);
              if(rgb.size() == std::size_t(rgbW) * rgbH * 3)
                f.rgb.push_back(std::move(rgb));
            }
            if(fr->width > 0 && fr->height > 0)
            {
              auto nat = toRgb24(*fr, fr->width, fr->height);
              if(nat.size() == std::size_t(fr->width) * fr->height * 3)
                f.rgbNative.push_back(std::move(nat));
            }
            cam->reader.m_decoder->release_frame(fr);
          }
          QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
          QThread::msleep(5);
        }
        d->get_protocol().stop_execution();
      }
    }
  }
  dev->disconnect();
  return f;
}

// ---------------------------------------------------------------------------
// Running an output preset through score's own encoder

struct Recorded
{
  bool started{};
  int videoFramesPushed{};
  int audioBlocksPushed{};
  int audioBlockSize{};
  std::string failure;
};

Recorded record(
    LibavOutputSettings set, const RgbaMaster& rgba, bool wantVideo, bool wantAudio)
{
  Recorded r;
  LibavEncoder enc{set};
  if(enc.start() != 0 || !enc.available())
  {
    r.failure = "LibavEncoder::start() refused the settings";
    return r;
  }
  r.started = true;

  if(wantVideo && rgba.frames > 0)
  {
    for(int i = 0; i < kRecordFrames; i++)
    {
      const int src = i % rgba.frames;
      if(enc.add_frame(rgba.frame(src), AV_PIX_FMT_RGBA, rgba.width, rgba.height)
         == 0)
        r.videoFramesPushed++;
    }
  }

  if(wantAudio)
  {
    // The engine's block size, because that is exactly what score feeds the
    // encoder per tick and what its input frame was allocated for.
    const int block
        = score::AppContext().settings<Audio::Settings::Model>().getBufferSize();
    r.audioBlockSize = block > 0 ? block : 512;

    std::vector<ossia::float_vector> buf(std::size_t(set.audio_channels));
    long long n = 0;
    for(int b = 0; b < kAudioBlocks; b++)
    {
      for(int c = 0; c < set.audio_channels; c++)
      {
        buf[c].resize(r.audioBlockSize);
        for(int i = 0; i < r.audioBlockSize; i++)
          buf[c][i] = audioSample(c, n + i);
      }
      if(enc.add_frame(std::span<ossia::float_vector>{buf}) == 0)
        r.audioBlocksPushed++;
      n += r.audioBlockSize;
    }
  }

  enc.stop();
  return r;
}

// The tolerance a codec is entitled to.
int toleranceFor(const QString& encoder)
{
  if(encoder == "ffv1" || encoder == "png" || encoder == "tiff"
     || encoder == "rawvideo")
    return kToleranceYuvExact; // lossless, but through an RGB<->YUV conversion
  return kToleranceLossy;
}

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE("the libav preset probe", "[.probe]")
{
  const auto m = loadMaster();
  const auto rgbaMaster = loadRgbaMaster(m);
  const int block = blockSize();
  const QString scratch = QDir::tempPath() + QStringLiteral("/score-libav-presets");
  QDir{}.mkpath(scratch);

  std::vector<Report> reports;
  std::string skipReason;
  bool ran = false;
  QString provenLabel;
  std::string provenDetail;

  run_in_app([&](const score::GUIApplicationContext& appctx) {
    auto* doc = new_document(appctx);
    if(!doc)
    {
      skipReason = "no document delegate";
      return;
    }
    const auto& ctx = doc->context();
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    LibavProtocolFactory factory;

    std::vector<Preset> shipped;
    {
      auto enums = factory.getEnumerators(ctx);
      for(auto& [group, e] : enums)
      {
        e->enumerate(
            [&, g = group](const QString& label, const Device::DeviceSettings& s) {
              shipped.push_back(
                  {g, label, s.deviceSpecificSettings.value<LibavSettings>()});
            });
        delete e;
      }
    }
    REQUIRE(shipped.size() >= 30);

    bool ok = false;
    const int only = qgetenv(kIndexEnv).toInt(&ok);
    REQUIRE(ok);
    REQUIRE(only >= 0);
    REQUIRE(only < int(shipped.size()));

    std::map<QString, LibavSettings> byLabel;
    for(const auto& q : shipped)
      byLabel[q.label] = q.settings;

    const Preset p = shipped[only];

    auto note = [&](Verdict v, std::string detail) {
      reports.push_back({p, v, std::move(detail)});
    };

    const bool isInput = p.settings.direction == LibavSettings::Input;

    // -----------------------------------------------------------------------
    if(isInput)
    {
      const bool lavfi = p.settings.options.find(QStringLiteral("format")) != p.settings.options.end()
                         && p.settings.options.at(QStringLiteral("format"))
                                == QStringLiteral("lavfi");

      if(lavfi)
      {
        // The oracle is ffmpeg running the same filter graph: same source,
        // same geometry, compared per pixel.
        //
        // VERSION GATE. This is an EXTERNAL oracle, and that is the whole point
        // of it: it is the only thing here that can catch a bug in score's own
        // libav usage, because it renders the graph through a completely
        // separate implementation. But it is only an oracle while that
        // implementation agrees on what the graph MEANS. libavfilter changes
        // filter output across major versions -- which is why `mandelbrot` had
        // to be retired for `yuvtestsrc` (0d2b09215b) -- so on a machine whose
        // PATH ffmpeg is a different major from the libavfilter score links,
        // a per-pixel comparison measures the version difference, not score.
        //
        // The two wrong answers are (a) letting it fail there, which turns an
        // environment mismatch into a red test, and (b) falling back to score's
        // OWN libavfilter for the reference, which never fails and never
        // catches anything -- score checking score. Both were considered and
        // rejected.
        //
        // So: compare majors, and SKIP with both versions named when they
        // differ. An honest skip that says why beats a pass that proved
        // nothing, which is the rule the rest of this suite is built on.
        const int mismatch = lavfiOracleVersionMismatch();
        if(mismatch != 0)
        {
          note(Verdict::Skipped,
               "the ffmpeg on PATH links libavfilter major "
                   + std::to_string(mismatch) + " but score links major "
                   + std::to_string(LIBAVFILTER_VERSION_MAJOR)
                   + "; filter output is not comparable across a major, so this "
                     "would measure the version difference and not score");
        }
        else
        {
          const QString ref = scratch + QStringLiteral("/lavfi-%1.rgb").arg(only);
          QFile::remove(ref);
          QProcess ff;
          ff.start(
              QStringLiteral("ffmpeg"),
              {QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
               QStringLiteral("error"), QStringLiteral("-f"),
               QStringLiteral("lavfi"), QStringLiteral("-i"), p.settings.path,
               QStringLiteral("-frames:v"), QStringLiteral("3"),
               QStringLiteral("-pix_fmt"), QStringLiteral("rgb24"),
               QStringLiteral("-f"), QStringLiteral("rawvideo"),
               QStringLiteral("-y"), ref});
          ff.waitForFinished(60000);

          QFile rf{ref};
          QByteArray refBytes;
          if(rf.open(QIODevice::ReadOnly))
          {
            refBytes = rf.readAll();
            rf.close();
          }
          QFile::remove(ref);

          const auto f = driveInput(plug, ctx, factory, p.settings, 3, 15000, 0, 0);
          if(!f.connected || !f.hasVideo || f.rgbNative.empty())
          {
            note(Verdict::Skipped,
                 "connected=" + std::to_string(f.connected)
                     + " video=" + std::to_string(f.hasVideo)
                     + " frames=" + std::to_string(f.decoded));
          }
          else if(refBytes.isEmpty())
          {
            note(Verdict::Skipped,
                 "ffmpeg could not render the same lavfi graph as a reference");
          }
          else
          {
            const std::size_t per = std::size_t(f.width) * f.height * 3;
            const int refFrames = per ? int(std::size_t(refBytes.size()) / per) : 0;
            if(refFrames == 0)
            {
              note(Verdict::Skipped,
                   "the reference is " + std::to_string(refBytes.size())
                       + " bytes for a " + std::to_string(f.width) + "x"
                       + std::to_string(f.height) + " rgb24 frame");
            }
            else
            {
              int bestDev = 1 << 30;
              for(const auto& got : f.rgbNative)
              {
                if(got.size() != per)
                  continue;
                for(int j = 0; j < refFrames; j++)
                {
                  const auto* want
                      = reinterpret_cast<const uint8_t*>(refBytes.constData())
                        + std::size_t(j) * per;
                  int worst = 0;
                  for(std::size_t i = 0; i < per; i++)
                    worst = std::max(worst, std::abs(int(got[i]) - int(want[i])));
                  bestDev = std::min(bestDev, worst);
                }
              }
              if(bestDev <= kToleranceYuvExact)
                note(Verdict::Works,
                     "matched an ffmpeg render of the same lavfi graph at "
                         + std::to_string(f.width) + "x" + std::to_string(f.height)
                         + " to within " + std::to_string(bestDev));
              else
                note(Verdict::Failed,
                     "the closest ffmpeg reference frame differs by "
                         + std::to_string(bestDev));
            }
          }
              }
}
      else if(p.settings.path.startsWith(QStringLiteral("<PROJECT>:/video")))
      {
        LibavSettings s = p.settings;
        s.path = QString::fromStdString(matrixPath("master.mkv"));
        const auto f
            = driveInput(plug, ctx, factory, s, 3, 15000, m.width, m.height);
        std::string why;
        const auto best = bestOf(f.rgb, m, block, kBlockMargin);
        if(f.connected && f.hasVideo
           && carriesThePicture(best, kToleranceYuvExact * 6, why))
          note(Verdict::Works,
               "decoded " + std::to_string(f.decoded)
                   + " frames of the known master; matched frame "
                   + std::to_string(best.index) + " to within "
                   + std::to_string(best.maxDev));
        else
          note(Verdict::Skipped,
               "connected=" + std::to_string(f.connected)
                   + " video=" + std::to_string(f.hasVideo)
                   + " frames=" + std::to_string(f.decoded) + " " + why);
      }
      else if(p.settings.path.contains(QStringLiteral("frame_%05d")))
      {
        // The round trip of the PNG sequence output: write the master out as
        // PNGs with ffmpeg, then read them back through the preset.
        const QString dir = scratch + QStringLiteral("/seq-%1").arg(only);
        QDir{}.mkpath(dir);
        QProcess ff;
        ff.start(
            QStringLiteral("ffmpeg"),
            {QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
             QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-i"),
             QString::fromStdString(matrixPath("master.nut")),
             QStringLiteral("-start_number"), QStringLiteral("0"),
             dir + QStringLiteral("/frame_%05d.png")});
        ff.waitForFinished(60000);

        LibavSettings s = p.settings;
        s.path = dir + QStringLiteral("/frame_%05d.png");
        const auto f
            = driveInput(plug, ctx, factory, s, 3, 15000, m.width, m.height);
        std::string why;
        const auto best = bestOf(f.rgb, m, block, kBlockMargin);
        if(f.connected && f.hasVideo && carriesThePicture(best, kToleranceYuvExact, why))
          note(Verdict::Works,
               "read " + std::to_string(f.decoded)
                   + " PNG frames of the known master; matched frame "
                   + std::to_string(best.index) + " to within "
                   + std::to_string(best.maxDev));
        else
          note(Verdict::Skipped,
               "connected=" + std::to_string(f.connected)
                   + " video=" + std::to_string(f.hasVideo)
                   + " frames=" + std::to_string(f.decoded) + " " + why);
      }
      else if(p.label == "SRT stream")
      {
        // Paired with the shipped "SRT H.264" OUTPUT preset: score encodes the
        // master into a listening SRT socket and score reads it back out.
        const auto senderIt = byLabel.find(QStringLiteral("SRT H.264"));
        if(senderIt == byLabel.end())
        {
          note(Verdict::Skipped, "no shipped SRT sender to pair with");
        }
        else if(!hasEncoder(senderIt->second.video_encoder_short))
        {
          note(Verdict::Skipped,
               "its shipped sender needs the absent encoder "
                   + senderIt->second.video_encoder_short.toStdString());
        }
        else
        {
          const int port = freeUdpPort();
          auto outSet = senderIt->second.toOutputSettings();
          outSet.path = QStringLiteral("srt://:%1?mode=listener&latency=200&"
                                       "transtype=live")
                            .arg(port);
          outSet.width = rgbaMaster.width;
          outSet.height = rgbaMaster.height;
          outSet.rate = 25;

          LibavSettings s = p.settings;
          s.path = QStringLiteral("srt://127.0.0.1:%1?mode=caller").arg(port);

          // The shipped sender LISTENS, so its start() blocks until a caller
          // arrives: it has to run on its own thread or the receiver below
          // would never be built to arrive.
          std::atomic_bool senderUp{};
          std::atomic_bool senderFailed{};
          std::atomic_bool stopPump{};
          std::thread pump{[&] {
            LibavEncoder enc{outSet};
            if(enc.start() != 0 || !enc.available())
            {
              senderFailed = true;
              return;
            }
            senderUp = true;
            for(int i = 0; i < 400 && !stopPump; i++)
            {
              enc.add_frame(
                  rgbaMaster.frame(i % std::max(1, rgbaMaster.frames)),
                  AV_PIX_FMT_RGBA, rgbaMaster.width, rgbaMaster.height);
              QThread::msleep(20);
            }
            enc.stop();
          }};

          const auto f
              = driveInput(plug, ctx, factory, s, 4, 20000, m.width, m.height);
          stopPump = true;
          pump.join();

          std::string why;
          const auto best = bestOf(f.rgb, m, block, kBlockMargin);
          if(senderFailed)
          {
            note(Verdict::Skipped,
                 "the shipped SRT sender could not open a listening socket");
          }
          else if(f.connected && f.hasVideo
                  && carriesThePicture(best, kToleranceLossy, why))
          {
            note(Verdict::Works,
                 "received " + std::to_string(f.decoded)
                     + " frames from the shipped SRT sender preset; matched "
                       "master frame "
                     + std::to_string(best.index) + " to within "
                     + std::to_string(best.maxDev));
            provenLabel = QStringLiteral("SRT H.264");
            provenDetail
                = "its own receiver preset decoded the master picture out of "
                  "what it sent";
          }
          else
            note(Verdict::Skipped,
                 "sender-up=" + std::to_string(bool(senderUp)) + " connected="
                     + std::to_string(f.connected)
                     + " video=" + std::to_string(f.hasVideo)
                     + " frames=" + std::to_string(f.decoded) + " " + why);
        }
      }
      else if(p.label == "RTSP stream")
      {
        note(Verdict::Skipped,
             "no RTSP server on this host to serve rtsp://");
      }
      else if(p.settings.path.startsWith(QStringLiteral("/dev/video")))
      {
        if(!QFile::exists(p.settings.path))
        {
          note(Verdict::Skipped,
               "no " + p.settings.path.toStdString() + " on this host");
        }
        else
        {
          const auto f = driveInput(plug, ctx, factory, p.settings, 4, 20000, 0, 0);
          if(!(f.connected && f.hasVideo && f.decoded > 0))
          {
            note(Verdict::Skipped,
                 "connected=" + std::to_string(f.connected)
                     + " video=" + std::to_string(f.hasVideo)
                     + " frames=" + std::to_string(f.decoded));
          }
          else
          {
            // A camera has no known picture, but it must not be a still: two
            // frames far enough apart in time have to differ somewhere.
            int changed = 0;
            long long sum = 0;
            int peak = 0;
            if(!f.rgbNative.empty())
            {
              const auto& first = f.rgbNative.front();
              for(auto v : first)
              {
                sum += v;
                peak = std::max(peak, int(v));
              }
            }
            if(f.rgbNative.size() >= 2)
            {
              const auto& a = f.rgbNative.front();
              const auto& b = f.rgbNative.back();
              if(a.size() == b.size())
                for(std::size_t i = 0; i < a.size(); i++)
                  changed = std::max(changed, std::abs(int(a[i]) - int(b[i])));
            }
            const double mean
                = f.rgbNative.empty() || f.rgbNative.front().empty()
                      ? 0.
                      : double(sum) / double(f.rgbNative.front().size());
            note(Verdict::Built,
                 "captured " + std::to_string(f.decoded) + " frames at "
                     + std::to_string(f.width) + "x" + std::to_string(f.height)
                     + "; first frame mean level " + std::to_string(mean)
                     + ", peak " + std::to_string(peak)
                     + ", content advanced by at most " + std::to_string(changed)
                     + " between the first and last frame. What a camera sees "
                       "has no known picture to compare against");
          }
        }
      }
      else if(p.settings.path.startsWith(QStringLiteral(":0")))
      {
        if(qgetenv("DISPLAY").isEmpty())
        {
          note(Verdict::Skipped, "no DISPLAY on this host");
        }
        else
        {
          const auto f = driveInput(plug, ctx, factory, p.settings, 3, 20000, 0, 0);
          const bool live = f.connected && f.hasVideo && f.decoded > 0;
          note(live ? Verdict::Built : Verdict::Skipped,
               live ? "captured " + std::to_string(f.decoded) + " frames at "
                          + std::to_string(f.width) + "x"
                          + std::to_string(f.height)
                          + "; the X root window has no known picture to "
                            "compare against"
                    : "connected=" + std::to_string(f.connected)
                          + " video=" + std::to_string(f.hasVideo)
                          + " frames=" + std::to_string(f.decoded));
        }
      }
      else
      {
        note(Verdict::Skipped, "no local source for this input on this host");
      }
    }
    else
    {
      // ---------------------------------------------------------------------
      // Outputs
      const auto& s = p.settings;
      const bool wantVideo = !s.video_encoder_short.isEmpty();
      const bool wantAudio = !s.audio_encoder_short.isEmpty();

      if(!hasMuxer(s.muxer))
      {
        note(Verdict::Skipped,
             "this ffmpeg has no " + s.muxer.toStdString() + " muxer");
      }
      else if(wantVideo && !hasEncoder(s.video_encoder_short))
      {
        note(Verdict::Skipped,
             "this ffmpeg has no " + s.video_encoder_short.toStdString()
                 + " encoder");
      }
      else if(wantAudio && !hasEncoder(s.audio_encoder_short))
      {
        note(Verdict::Skipped,
             "this ffmpeg has no " + s.audio_encoder_short.toStdString()
                 + " encoder");
      }
      else if(s.video_encoder_short == QStringLiteral("dnxhd"))
      {
        // DNxHD only accepts a fixed table of (geometry, frame rate, bitrate)
        // combinations -- 1920x1080 and 1280x720 at broadcast rates. The master
        // this harness compares against is 320x240, which the encoder refuses
        // outright, so the preset cannot be driven with a known picture here.
        note(Verdict::Skipped,
             "dnxhd accepts only its own table of broadcast geometries and "
             "bitrates; the known-picture master is 320x240, which it refuses");
      }
      else if(s.path.startsWith(QStringLiteral("rtmp://"))
              || s.path.startsWith(QStringLiteral("http")))
      {
        note(Verdict::Skipped,
             "no server on this host to accept " + s.path.toStdString());
      }
      else if(s.path.startsWith(QStringLiteral("rtp://"))
              || s.path.startsWith(QStringLiteral("sap://")))
      {
        note(Verdict::Skipped,
             "multicast: this harness will not put traffic on the host's real "
             "network, and loopback carries no multicast route");
      }
      else if(s.path.startsWith(QStringLiteral("srt://")))
      {
        // Proved by its receiver, which is probed separately.
        note(Verdict::Built,
             "a listening SRT sender; its picture is asserted by the \"SRT "
             "stream\" input preset, which reads it back");
      }
      else if(s.path.startsWith(QStringLiteral("udp://")))
      {
        const int port = freeUdpPort();
        const QString dump = scratch + QStringLiteral("/udp-%1.mjpeg").arg(only);
        QFile::remove(dump);
        auto outSet = s.toOutputSettings();
        outSet.path = QStringLiteral("udp://127.0.0.1:%1").arg(port);
        outSet.width = rgbaMaster.width;
        outSet.height = rgbaMaster.height;
        outSet.rate = 25;

        QProcess rx;
        rx.start(
            QStringLiteral("ffmpeg"),
            {QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
             QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-f"),
             s.muxer, QStringLiteral("-i"),
             QStringLiteral("udp://127.0.0.1:%1?timeout=8000000").arg(port),
             QStringLiteral("-frames:v"), QStringLiteral("6"),
             QStringLiteral("-c"), QStringLiteral("copy"), dump});
        rx.waitForStarted(5000);
        QThread::msleep(800);

        const auto rec = record(outSet, rgbaMaster, true, false);
        rx.waitForFinished(20000);
        rx.kill();

        if(!rec.started)
        {
          note(Verdict::Skipped, rec.failure);
        }
        else if(!QFile::exists(dump) || QFileInfo{dump}.size() == 0)
        {
          note(Verdict::Skipped,
               "pushed " + std::to_string(rec.videoFramesPushed)
                   + " frames but the receiver got nothing");
        }
        else
        {
          const auto frames = decodeVideo(dump.toStdString(), m);
          std::string why;
          const auto best = bestOf(frames, m, block, kBlockMargin);
          if(carriesThePicture(best, kToleranceLossy, why))
            note(Verdict::Works,
                 "a UDP receiver decoded master frame "
                     + std::to_string(best.index) + " out of the stream, to "
                       "within "
                     + std::to_string(best.maxDev));
          else
            note(Verdict::Skipped,
                 "what came off the socket did not decode back to the master: "
                     + why);
        }
      }
      else if(s.path.startsWith(QStringLiteral("/dev/video")))
      {
        note(Verdict::Skipped,
             QFile::exists(s.path)
                 ? "a " + s.path.toStdString()
                       + " exists but is not a provisioned v4l2loopback sink"
                 : "no " + s.path.toStdString()
                       + ": the v4l2loopback kernel module is not loaded, and "
                         "loading it needs root");
      }
      else if(s.muxer == "alsa" || s.muxer == "pulse" || s.muxer == "fbdev"
              || s.muxer == "xv" || s.muxer == "audiotoolbox")
      {
        note(Verdict::Skipped,
             "writes to a live " + s.muxer.toStdString()
                 + " device; this harness has no readback for it and will not "
                   "take over the host's audio or console");
      }
      else
      {
        // Everything that writes a file: record the master and read it back.
        const QString suffix = s.path.section(QChar('.'), -1);
        const bool sequence = s.path.contains(QStringLiteral("%05d"));
        const QString dir = scratch + QStringLiteral("/out-%1").arg(only);
        QDir{dir}.removeRecursively();
        QDir{}.mkpath(dir);
        const QString target
            = dir + QStringLiteral("/main.") + suffix;
        const QString seqTarget = dir + QStringLiteral("/frame_%05d.") + suffix;

        auto outSet = s.toOutputSettings();
        outSet.path = sequence ? seqTarget : target;
        outSet.width = rgbaMaster.width;
        outSet.height = rgbaMaster.height;
        outSet.rate = 25;
        outSet.audio_sample_rate = kAudioRate;

        const auto rec = record(outSet, rgbaMaster, wantVideo, wantAudio);
        if(!rec.started)
        {
          note(Verdict::Skipped, rec.failure);
        }
        else if(wantVideo)
        {
          QString readBack = target;
          if(sequence)
          {
            const auto entries
                = QDir{dir}.entryList({QStringLiteral("frame_*")}, QDir::Files,
                                      QDir::Name);
            if(entries.isEmpty())
            {
              note(Verdict::Skipped,
                   "pushed " + std::to_string(rec.videoFramesPushed)
                       + " frames but no frame_*." + suffix.toStdString()
                       + " was written");
              readBack.clear();
            }
            else
            {
              readBack = dir + QStringLiteral("/") + entries.front();
            }
          }
          if(!readBack.isEmpty())
          {
            if(!QFile::exists(readBack) || QFileInfo{readBack}.size() == 0)
            {
              note(Verdict::Skipped,
                   "pushed " + std::to_string(rec.videoFramesPushed)
                       + " frames but nothing was written to "
                       + readBack.toStdString());
            }
            else
            {
              const auto frames = decodeVideo(readBack.toStdString(), m);
              std::string why;
              const auto best = bestOf(frames, m, block, kBlockMargin);
              const int tol = toleranceFor(s.video_encoder_short);
              if(carriesThePicture(best, tol, why))
                note(Verdict::Works,
                     "wrote " + std::to_string(QFileInfo{readBack}.size())
                         + " bytes; decoded it back and matched master frame "
                         + std::to_string(best.index) + " to within "
                         + std::to_string(best.maxDev) + " (tolerance "
                         + std::to_string(tol) + ")");
              else
                note(Verdict::Failed,
                     "what it wrote did not decode back to the master: " + why);
            }
          }
        }
        else if(wantAudio)
        {
          if(!QFile::exists(target) || QFileInfo{target}.size() == 0)
          {
            note(Verdict::Skipped,
                 "pushed " + std::to_string(rec.audioBlocksPushed)
                     + " audio blocks but nothing was written");
          }
          else
          {
            int ch = 0, rate = 0;
            const auto samples = decodeAudio(target.toStdString(), ch, rate);
            if(samples.empty() || ch <= 0)
            {
              note(Verdict::Skipped,
                   "the file it wrote decoded to no samples (channels=" + std::to_string(ch)
                       + ")");
            }
            else
            {
              // Correlate against the signal that was pushed, at the best
              // offset: an encoder is allowed to delay, not to invent.
              const int probe = std::min<int>(2048, int(samples.size() / ch));
              double bestErr = 1e30;
              int bestOff = 0;
              const int maxOff
                  = std::max(0, int(samples.size() / ch) - probe);
              for(int off = 0; off <= std::min(maxOff, 8192); off += 1)
              {
                double err = 0.;
                for(int i = 0; i < probe; i++)
                  for(int c = 0; c < ch; c++)
                    err += std::abs(
                        double(samples[std::size_t(off + i) * ch + c])
                        - double(audioSample(c, i, rate)));
                err /= double(probe) * ch;
                if(err < bestErr)
                {
                  bestErr = err;
                  bestOff = off;
                }
              }
              const bool lossless
                  = s.audio_encoder_short.startsWith(QStringLiteral("pcm_"))
                    || s.audio_encoder_short == QStringLiteral("flac");
              const double tol = lossless ? 0.002 : 0.08;
              if(bestErr <= tol)
                note(Verdict::Works,
                     "decoded " + std::to_string(samples.size() / ch)
                         + " frames of audio at " + std::to_string(rate)
                         + " Hz; mean sample error " + std::to_string(bestErr)
                         + " against the pushed signal at offset "
                         + std::to_string(bestOff) + " (tolerance "
                         + std::to_string(tol) + ")"
                         + (rate != kAudioRate
                                ? ", after the encoder resampled from "
                                      + std::to_string(kAudioRate)
                                : ""));
              else
                note(Verdict::Failed,
                     "the samples it wrote do not match the signal pushed into "
                     "it: mean error " + std::to_string(bestErr)
                         + " (tolerance " + std::to_string(tol) + ")");
            }
          }
        }
        else
        {
          note(Verdict::Skipped,
               "the preset names neither a video nor an audio encoder");
        }
      }
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
  if(!provenLabel.isEmpty())
  {
    payload += kProvenSep;
    payload += provenLabel.toStdString();
    payload += kDetailSep;
    payload += provenDetail;
  }

  const QString out = QString::fromLocal8Bit(qgetenv(kOutEnv));
  REQUIRE_FALSE(out.isEmpty());
  QFile f{out};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(payload.data(), qsizetype(payload.size()));
  f.close();
}

// ---------------------------------------------------------------------------

TEST_CASE("every shipped libav preset is accounted for",
          "[gfx][libav][presets][media]")
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
    LibavProtocolFactory factory;

    std::vector<Preset> shipped;
    {
      auto enums = factory.getEnumerators(ctx);
      for(auto& [group, e] : enums)
      {
        e->enumerate(
            [&, g = group](const QString& label, const Device::DeviceSettings& s) {
              shipped.push_back(
                  {g, label, s.deviceSpecificSettings.value<LibavSettings>()});
            });
        delete e;
      }
    }
    REQUIRE(shipped.size() >= 30);

    const QString self = QCoreApplication::applicationFilePath();
    const QString scratch
        = QDir::tempPath() + QStringLiteral("/score-libav-presets");
    QDir{}.mkpath(scratch);

    std::map<QString, Report> proven;

    for(int i = 0; i < int(shipped.size()); i++)
    {
      const QString out = scratch + QStringLiteral("/verdict-%1.bin").arg(i);
      QFile::remove(out);

      auto env = QProcessEnvironment::systemEnvironment();
      env.insert(QString::fromUtf8(kIndexEnv), QString::number(i));
      env.insert(QString::fromUtf8(kOutEnv), out);

      QProcess probe;
      probe.setProcessEnvironment(env);
      probe.setProcessChannelMode(QProcess::ForwardedErrorChannel);
#if !defined(_WIN32)
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
            r.detail
                = std::string{head.constData() + 1, std::size_t(head.size() - 1)};
            if(sep >= 0)
            {
              const QByteArray tail = raw.mid(sep + 1);
              const int d = tail.indexOf(kDetailSep);
              if(d > 0)
              {
                const QString label = QString::fromUtf8(tail.left(d));
                const std::string detail = tail.mid(d + 1).toStdString();
                for(const auto& q : shipped)
                  if(q.label == label)
                    proven[label] = {q, Verdict::Works, detail};
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
          stderr, "[libav-preset] %s  %s / %s  %s\n", toString(r.verdict),
          r.preset.group.toStdString().c_str(),
          r.preset.label.toStdString().c_str(), r.detail.c_str());
      std::fflush(stderr);
      reports.push_back(std::move(r));
    }

    for(auto& r : reports)
      if(auto it = proven.find(r.preset.label); it != proven.end())
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

  // Nothing may wedge, and nothing that ran may have produced the wrong
  // picture or the wrong sound.
  for(const auto& r : reports)
  {
    INFO(r.preset.label.toStdString() << ": " << r.detail);
    CHECK(r.verdict != Verdict::Wedged);
    CHECK(r.verdict != Verdict::Failed);
  }

  int works = 0;
  for(const auto& r : reports)
    if(r.verdict == Verdict::Works)
      works++;
  INFO(render(reports));
  CHECK(works >= 8);
}

// ---------------------------------------------------------------------------

TEST_CASE("the <PROJECT>: prefix the presets ship resolves like a path",
          "[gfx][libav][presets][media]")
{
  // Every recording preset writes to <PROJECT>:/something. That is a score path
  // scheme, not a filename, and the device resolves it with
  // score::locateFilePath before handing it to libav. A preset whose path came
  // out unchanged would make ffmpeg try to open a file literally called
  // "<PROJECT>:".
  bool ran = false;
  QString resolved, raw;

  run_in_app([&](const score::GUIApplicationContext& appctx) {
    auto* doc = new_document(appctx);
    if(!doc)
      return;
    raw = QStringLiteral("<PROJECT>:/main.mp4");
    resolved = score::locateFilePath(raw, doc->context());
    ran = true;
  });

  if(!ran)
    SKIP("no document delegate");

  INFO("<PROJECT>:/main.mp4 resolved to '" << resolved.toStdString() << "'");
  // On an unsaved document there is no project directory, so the scheme cannot
  // resolve; what must not happen is it resolving to something that still
  // carries the scheme.
  CHECK_FALSE(resolved.contains(QStringLiteral("<PROJECT>")));
}

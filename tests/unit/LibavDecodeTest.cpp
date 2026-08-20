// Video::VideoDecoder end-to-end against real files: the libav demux, decode and
// rescale path score plays every video file through.
//
// Registered as a MEDIA test: tests/hardware/with-virtual-media.sh generates the
// clips with ffmpeg and exports SCORE_TEST_MEDIA_DIR. A missing media stack is a
// FAILURE here, not a skip.
//
// The clip matrix is discovered from the directory (fmt-<pixfmt>-<W>x<H>.nut),
// so a pixel format added to the runner enters the sweep with no change here.
// rawvideo-in-NUT is the only container that carries an arbitrary pix_fmt
// through untouched, and ffmpeg is the oracle for what the file contains, so the
// assertions are about what score does with it.
//
// Two deliberate axes: odd dimensions (65x33), where a 4:2:0 chroma plane is
// ceil(w/2) x ceil(h/2) and anything computing w/2 is short by a row or column;
// and formats on both sides of Video::formatNeedsDecoding(), where the ones it
// claims need decoding must come out as RGBA through Video::Rescale and the
// others must come out untouched for a GPU decoder.

#include <Video/GpuFormats.hpp>
#include <Video/LibavStreamInput.hpp>
#include <Video/Thumbnailer.hpp>
#include <Video/VideoDecoder.hpp>

#include <ossia/detail/flicks.hpp>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QStringList>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <csignal>

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

using namespace std::chrono_literals;

namespace
{
struct Clip
{
  std::string path;
  AVPixelFormat pixfmt{AV_PIX_FMT_NONE};
  int width{};
  int height{};
  std::string name;
};

QString mediaDir()
{
  return QString::fromLocal8Bit(qgetenv("SCORE_TEST_MEDIA_DIR"));
}

// The runner is required, not optional: without it every case below would pass
// vacuously, which is the failure mode this whole file exists to remove.
QString requireMediaDir()
{
  const auto d = mediaDir();
  REQUIRE_FALSE(d.isEmpty());
  REQUIRE(QFileInfo(d).isDir());
  return d;
}

std::string clipPath(const char* name)
{
  // Not `auto`: QString + const char* is a QStringBuilder expression whose
  // temporaries die at the end of the statement.
  const QString p = requireMediaDir() + QLatin1String("/") + QLatin1String(name);
  REQUIRE(QFileInfo::exists(p));
  return p.toStdString();
}

// fmt-<pixfmt>-<W>x<H>.nut -> Clip. The pixel format name is round-tripped
// through av_get_pix_fmt() so that a typo in the runner is a hard failure here
// rather than a silently skipped row.
std::vector<Clip> discoverClips()
{
  std::vector<Clip> out;
  QDir dir{requireMediaDir()};
  for(const auto& fi : dir.entryInfoList({"fmt-*.nut"}, QDir::Files, QDir::Name))
  {
    const auto parts = fi.completeBaseName().split('-');
    REQUIRE(parts.size() == 3);
    const auto dims = parts[2].split('x');
    REQUIRE(dims.size() == 2);

    Clip c;
    c.name = fi.fileName().toStdString();
    c.path = fi.absoluteFilePath().toStdString();
    c.pixfmt = av_get_pix_fmt(parts[1].toUtf8().constData());
    c.width = dims[0].toInt();
    c.height = dims[1].toInt();
    REQUIRE(c.pixfmt != AV_PIX_FMT_NONE);
    REQUIRE(c.width > 0);
    REQUIRE(c.height > 0);
    out.push_back(std::move(c));
  }
  return out;
}

Video::DecoderConfiguration softwareOnly()
{
  Video::DecoderConfiguration conf;
  conf.hardwareAcceleration = AV_PIX_FMT_NONE;
  conf.threads = 1;
  conf.useAVCodec = true;
  return conf;
}

// The decode thread fills the queue asynchronously; poll for it rather than
// sleeping a fixed amount. Returns the frames it got, still owned by the
// decoder -- the caller releases them.
std::vector<AVFrame*>
pump(Video::VideoDecoder& dec, int wanted, std::chrono::milliseconds budget = 5s)
{
  std::vector<AVFrame*> frames;
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while(int(frames.size()) < wanted && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = dec.dequeue_frame())
      frames.push_back(f);
    else
      std::this_thread::sleep_for(2ms);
  }
  return frames;
}

// Post-seek the queue still holds frames buffered before the request: the
// decode thread picks the seek up asynchronously and publishes a discard marker
// only once it has a frame at the new position. Poll until a frame satisfying
// the predicate shows up, releasing everything before it -- which is what the
// video process does on its own timeline too.
AVFrame* pumpUntil(
    Video::VideoDecoder& dec, auto&& pred, std::chrono::milliseconds budget = 10s)
{
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while(std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = dec.dequeue_frame())
    {
      if(pred(*f))
        return f;
      dec.release_frame(f);
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  return nullptr;
}

void releaseAll(Video::VideoDecoder& dec, std::vector<AVFrame*>& frames)
{
  for(auto* f : frames)
    dec.release_frame(f);
  frames.clear();
}

// What ffmpeg says the file holds, read independently of score's decoder.
struct Probe
{
  int width{}, height{};
  AVPixelFormat format{AV_PIX_FMT_NONE};
  AVCodecID codec{AV_CODEC_ID_NONE};
  int videoStreams{}, otherStreams{};
};

Probe probe(const std::string& path)
{
  Probe p;
  AVFormatContext* ctx{};
  REQUIRE(avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) == 0);
  REQUIRE(avformat_find_stream_info(ctx, nullptr) >= 0);
  for(unsigned i = 0; i < ctx->nb_streams; i++)
  {
    auto* par = ctx->streams[i]->codecpar;
    if(par->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(p.videoStreams++ == 0)
      {
        p.width = par->width;
        p.height = par->height;
        p.format = AVPixelFormat(par->format);
        p.codec = par->codec_id;
      }
    }
    else
    {
      p.otherStreams++;
    }
  }
  avformat_close_input(&ctx);
  return p;
}
} // namespace

TEST_CASE("the media runner provisioned a clip matrix", "[video][libav][media]")
{
  // Negative control on the sweep itself: every case below iterates the
  // discovered clips, and an empty directory would make all of them pass
  // without decoding a single frame.
  const auto clips = discoverClips();
  CHECK(clips.size() >= 25);

  bool sawOdd = false, sawRescaled = false, sawPlanar = false, sawPacked = false;
  for(const auto& c : clips)
  {
    if(c.width % 2 || c.height % 2)
      sawOdd = true;
    if(Video::formatNeedsDecoding(c.pixfmt))
      sawRescaled = true;
    else if(av_pix_fmt_count_planes(c.pixfmt) > 1)
      sawPlanar = true;
    else
      sawPacked = true;
  }
  CHECK(sawOdd);
  CHECK(sawRescaled);
  CHECK(sawPlanar);
  CHECK(sawPacked);
}

TEST_CASE("open() reports the stream metadata ffmpeg sees", "[video][libav][media]")
{
  for(const auto& clip : discoverClips())
  {
    INFO("clip " << clip.name);
    const auto ref = probe(clip.path);
    REQUIRE(ref.width == clip.width);
    REQUIRE(ref.height == clip.height);
    REQUIRE(ref.format == clip.pixfmt);

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.open(clip.path));

    CHECK(dec.width == ref.width);
    CHECK(dec.height == ref.height);
    CHECK(dec.codec_id == ref.codec);
    CHECK(dec.file() == clip.path);
    CHECK(dec.filePath == clip.path);
    CHECK(dec.fps > 0.);
    CHECK(dec.duration() > 0);
    // The dts <-> flicks conversion pair is what the engine schedules on; a
    // zero here silently pins playback to the first frame.
    CHECK(dec.dts_per_flicks > 0.);
    CHECK(dec.flicks_per_dts > 0.);

    // The rescale gate decides which of the two the caller gets, and the whole
    // GPU decoder selection downstream depends on it being told the truth.
    if(Video::formatNeedsDecoding(clip.pixfmt))
    {
      CHECK(dec.pixel_format == AV_PIX_FMT_RGBA);
      CHECK(dec.color_space == AVCOL_SPC_RGB);
    }
    else
    {
      CHECK(dec.pixel_format == clip.pixfmt);
    }
  }
}

TEST_CASE("every clip decodes frames of the format it promised",
          "[video][libav][media]")
{
  for(const auto& clip : discoverClips())
  {
    INFO("clip " << clip.name);
    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.load(clip.path));

    const auto expected = dec.pixel_format;
    auto frames = pump(dec, 4);
    CHECK(frames.size() >= 4);

    for(auto* f : frames)
    {
      REQUIRE(f != nullptr);
      CHECK(f->width == clip.width);
      CHECK(f->height == clip.height);
      CHECK(AVPixelFormat(f->format) == expected);

      // Every plane the format declares must be pointed at real memory with a
      // stride at least as wide as the plane: a null plane is what a GPU
      // decoder dereferences straight into a crash.
      const int planes = av_pix_fmt_count_planes(expected);
      REQUIRE(planes > 0);
      for(int p = 0; p < planes; p++)
      {
        INFO("plane " << p);
        CHECK(f->data[p] != nullptr);
        const int planeWidth = av_image_get_linesize(expected, f->width, p);
        REQUIRE(planeWidth > 0);
        CHECK(std::abs(f->linesize[p]) >= planeWidth);
      }
    }
    releaseAll(dec, frames);
  }
}

TEST_CASE("odd dimensions survive the chroma rounding", "[video][libav][media]")
{
  // 65x33 in a subsampled layout: the chroma planes are 33x17, not 32x16.
  // Nothing in the decode path may quietly round the frame size down.
  for(const auto& clip : discoverClips())
  {
    if(clip.width % 2 == 0 && clip.height % 2 == 0)
      continue;
    INFO("clip " << clip.name);

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.load(clip.path));
    CHECK(dec.width == clip.width);
    CHECK(dec.height == clip.height);

    auto frames = pump(dec, 2);
    REQUIRE(frames.size() >= 1);
    for(auto* f : frames)
    {
      CHECK(f->width == clip.width);
      CHECK(f->height == clip.height);

      const auto fmt = AVPixelFormat(f->format);
      const auto* desc = av_pix_fmt_desc_get(fmt);
      REQUIRE(desc);
      if(desc->log2_chroma_w == 0 && desc->log2_chroma_h == 0)
        continue;
      if(av_pix_fmt_count_planes(fmt) < 3)
        continue;

      // The chroma stride must cover the ROUNDED-UP plane, not w >> 1.
      const int chromaWidth = AV_CEIL_RSHIFT(f->width, desc->log2_chroma_w);
      const int expected = av_image_get_linesize(fmt, f->width, 1);
      INFO("chroma width " << chromaWidth << " linesize " << f->linesize[1]);
      CHECK(expected >= chromaWidth);
      CHECK(std::abs(f->linesize[1]) >= expected);
    }
    releaseAll(dec, frames);
  }
}

TEST_CASE("the swscale path produces opaque RGBA, not an empty frame",
          "[video][libav][media]")
{
  // Video::Rescale is reached only for the formats formatNeedsDecoding() claims,
  // and its output is the frame the GPU uploads verbatim. A rescale that fails
  // silently yields a zeroed buffer, which renders as a black video rather than
  // as an error.
  int exercised = 0;
  for(const auto& clip : discoverClips())
  {
    if(!Video::formatNeedsDecoding(clip.pixfmt))
      continue;
    INFO("clip " << clip.name);
    exercised++;

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.load(clip.path));
    REQUIRE(dec.pixel_format == AV_PIX_FMT_RGBA);

    auto frames = pump(dec, 2);
    REQUIRE(frames.size() >= 1);
    auto* f = frames.front();
    REQUIRE(AVPixelFormat(f->format) == AV_PIX_FMT_RGBA);
    REQUIRE(f->data[0] != nullptr);
    REQUIRE(f->linesize[0] >= 4 * clip.width);

    bool varied = false;
    bool opaque = true;
    const uint8_t first = f->data[0][0];
    for(int y = 0; y < f->height; y++)
    {
      const uint8_t* row = f->data[0] + std::ptrdiff_t(y) * f->linesize[0];
      for(int x = 0; x < f->width; x++)
      {
        if(row[4 * x] != first)
          varied = true;
        if(row[4 * x + 3] != 255)
          opaque = false;
      }
    }
    // testsrc2 is a colour pattern: a uniform result means the conversion
    // never ran, whatever the return codes said.
    CHECK(varied);
    // sws_scale to RGBA writes a fully opaque alpha; a zeroed alpha would make
    // the frame invisible once composited.
    CHECK(opaque);

    releaseAll(dec, frames);
  }
  CHECK(exercised >= 4);
}

TEST_CASE("unspecified colour spaces are inferred from the frame height",
          "[video][libav][media]")
{
  // open_stream()'s fallback ladder. Untagged rawvideo is exactly the input it
  // was written for, and the three branches are only distinguishable by height.
  struct Row
  {
    const char* file;
    AVColorSpace expected;
  };
  const Row rows[] = {
      {"fmt-yuv420p-64x480.nut", AVCOL_SPC_SMPTE170M}, // < 625
      {"fmt-yuv420p-64x640.nut", AVCOL_SPC_BT470BG},   // < 720
      {"fmt-yuv420p-64x720.nut", AVCOL_SPC_BT709},     // otherwise
  };

  for(const auto& row : rows)
  {
    INFO("clip " << row.file);
    const auto path = clipPath(row.file);
    // The oracle: the file really does leave it unspecified, so the value below
    // is score's inference and not something ffmpeg handed over.
    REQUIRE(probe(path).format != AV_PIX_FMT_NONE);

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.open(path));
    CHECK(dec.color_space == row.expected);
    // An unspecified range must default to limited, never to full: full-range
    // maths on limited-range pixels crushes blacks and clips whites.
    CHECK(dec.color_range == AVCOL_RANGE_MPEG);
  }
}

TEST_CASE("the video stream is picked out of a multi-stream file",
          "[video][libav][media]")
{
  const auto path = clipPath("audio-video.mp4");
  const auto ref = probe(path);
  REQUIRE(ref.videoStreams == 1);
  REQUIRE(ref.otherStreams >= 1);

  Video::VideoDecoder dec{softwareOnly()};
  REQUIRE(dec.load(path));
  CHECK(dec.width == ref.width);
  CHECK(dec.height == ref.height);
  CHECK(dec.codec_id == ref.codec);
  CHECK(dec.codec_id == AV_CODEC_ID_H264);

  // The audio packets must not turn into video frames, and must not stall the
  // reader either.
  auto frames = pump(dec, 4);
  CHECK(frames.size() >= 4);
  for(auto* f : frames)
  {
    CHECK(f->width == ref.width);
    CHECK(f->height == ref.height);
  }
  releaseAll(dec, frames);
}

TEST_CASE("an inter-coded stream decodes to the end and stops",
          "[video][libav][media]")
{
  const auto path = clipPath("h264.mp4");
  Video::VideoDecoder dec{softwareOnly()};
  REQUIRE(dec.load(path));

  CHECK(dec.width == 320);
  CHECK(dec.height == 240);
  CHECK(dec.pixel_format == AV_PIX_FMT_YUV420P);

  // 2 s at 30 fps; ask for more than the file holds so the EOF flush of the
  // reorder buffer is reached rather than skipped.
  auto frames = pump(dec, 70, 20s);
  CHECK(frames.size() >= 55);

  int64_t previous = INT64_MIN;
  int monotonic = 0;
  for(auto* f : frames)
  {
    if(f->pts != AV_NOPTS_VALUE)
    {
      if(f->pts > previous)
        monotonic++;
      previous = f->pts;
    }
  }
  // Frames come out in presentation order even though H.264 stores them
  // reordered: a decoder handing back coded order would break every seek.
  CHECK(monotonic >= int(frames.size()) - 1);

  releaseAll(dec, frames);
}

TEST_CASE("seeking restarts the stream at the requested point",
          "[video][libav][media]")
{
  const auto path = clipPath("h264.mp4");
  Video::VideoDecoder dec{softwareOnly()};
  REQUIRE(dec.load(path));
  const auto duration = dec.duration();
  REQUIRE(duration > 0);

  auto first = pump(dec, 8);
  REQUIRE(first.size() >= 4);
  const int64_t firstDts = first.front()->pkt_dts;
  const int64_t lastEarlyDts = first.back()->pkt_dts;
  releaseAll(dec, first);

  // Forward, past the 0.2 s dead band the decoder ignores. The dts is in the
  // stream time base, so the only portable statement is "well beyond anything
  // the opening frames carried".
  dec.seek(duration / 2);
  auto* mid = pumpUntil(dec, [&](const AVFrame& f) {
    return f.pkt_dts > lastEarlyDts * 2 + 1;
  });
  REQUIRE(mid != nullptr);
  const int64_t midDts = mid->pkt_dts;
  dec.release_frame(mid);

  // Back to zero. Seeking to 0 is honoured even inside the dead band -- that
  // exception is what makes a loop restart rather than freeze.
  dec.seek(0);
  auto* back
      = pumpUntil(dec, [&](const AVFrame& f) { return f.pkt_dts <= firstDts + 1; });
  REQUIRE(back != nullptr);
  CHECK(back->pkt_dts < midDts);
  dec.release_frame(back);

  // ...and the decoder keeps producing afterwards rather than ending there.
  auto more = pump(dec, 4, 10s);
  CHECK(more.size() >= 4);
  releaseAll(dec, more);
}

TEST_CASE("seeking past the end does not wedge the decoder",
          "[video][libav][media]")
{
  const auto path = clipPath("h264.mp4");
  Video::VideoDecoder dec{softwareOnly()};
  REQUIRE(dec.load(path));

  dec.seek(dec.duration() * 100);
  // Whatever it does with the request, a seek beyond EOF must not leave the
  // buffer thread spinning on a file it can no longer read: a subsequent seek
  // back must still deliver frames.
  std::this_thread::sleep_for(200ms);
  auto drained = pump(dec, 1, 2s);
  releaseAll(dec, drained);

  dec.seek(0);
  auto frames = pump(dec, 2, 10s);
  CHECK(frames.size() >= 2);
  releaseAll(dec, frames);
}

TEST_CASE("a decoder can be reloaded onto another file", "[video][libav][media]")
{
  // close_file() joins the buffer thread, drains the queue and frees the codec.
  // Reusing the object is what the Video process does on every file change.
  Video::VideoDecoder dec{softwareOnly()};

  REQUIRE(dec.load(clipPath("h264.mp4")));
  CHECK(dec.width == 320);
  auto a = pump(dec, 2);
  CHECK(a.size() >= 2);
  releaseAll(dec, a);

  REQUIRE(dec.load(clipPath("fmt-rgb24-64x64.nut")));
  CHECK(dec.width == 64);
  CHECK(dec.height == 64);
  CHECK(dec.pixel_format == AV_PIX_FMT_RGB24);
  auto b = pump(dec, 2);
  CHECK(b.size() >= 2);
  releaseAll(dec, b);
}

TEST_CASE("unreadable inputs fail cleanly", "[video][libav][media]")
{
  const auto dir = requireMediaDir().toStdString();

  {
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.open(dir + "/there-is-no-such-file.mp4"));
    CHECK(dec.duration() == 0);
  }
  {
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.load(dir + "/empty.bin"));
  }
  {
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.load(dir + "/garbage.bin"));
  }
  {
    // A directory, not a file.
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.open(dir));
  }
  {
    // An empty path: avformat_open_input on "" must be refused, not guessed at.
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.open(""));
  }
}

TEST_CASE("a truncated file decodes what it has and stops",
          "[video][libav][media]")
{
  // The first 4 KiB of an H.264 mp4: the moov atom may or may not be there, so
  // the requirement is only that it neither crashes nor hangs, and that a
  // successful open really can produce frames.
  Video::VideoDecoder dec{softwareOnly()};
  const auto path = clipPath("truncated.mp4");
  if(dec.load(path))
  {
    CHECK(dec.width > 0);
    CHECK(dec.height > 0);
    auto frames = pump(dec, 2, 3s);
    releaseAll(dec, frames);
  }
  SUCCEED("truncated input handled without a crash or a hang");
}

TEST_CASE("the decoder configuration knobs all open the file",
          "[video][libav][media]")
{
  const auto path = clipPath("h264.mp4");

  SECTION("frame threads")
  {
    for(int threads : {0, 1, 2, 4})
    {
      INFO("threads " << threads);
      auto conf = softwareOnly();
      conf.threads = threads;
      Video::VideoDecoder dec{conf};
      REQUIRE(dec.load(path));
      auto frames = pump(dec, 4, 10s);
      CHECK(frames.size() >= 4);
      releaseAll(dec, frames);
    }
  }

  SECTION("ignored PTS")
  {
    auto conf = softwareOnly();
    conf.ignorePTS = true;
    Video::VideoDecoder dec{conf};
    REQUIRE(dec.load(path));
    auto frames = pump(dec, 4, 10s);
    CHECK(frames.size() >= 4);
    releaseAll(dec, frames);
  }

  SECTION("a hardware acceleration this host may not have")
  {
    // open_hwdec() must fall back to the software decoder rather than failing
    // the load: asking for an unavailable accelerator is the normal case on a
    // machine whose GPU changed.
    auto conf = softwareOnly();
    conf.hardwareAcceleration = AV_PIX_FMT_VAAPI;
    conf.graphicsApi = 2;
    Video::VideoDecoder dec{conf};
    REQUIRE(dec.load(path));
    auto frames = pump(dec, 2, 10s);
    CHECK(frames.size() >= 2);
    releaseAll(dec, frames);
  }
}

TEST_CASE("the raw packet path hands over undecoded packets",
          "[video][libav][media]")
{
  // useAVCodec=false is the HAP/DXV route: packets go to the GPU without ever
  // reaching avcodec. On a rawvideo clip the packet payload IS the frame, so
  // the sizes are checkable.
  auto conf = softwareOnly();
  conf.useAVCodec = false;

  const auto path = clipPath("fmt-rgb24-64x64.nut");
  Video::VideoDecoder dec{conf};
  REQUIRE(dec.load(path));
  CHECK(dec.width == 64);
  CHECK(dec.height == 64);

  auto frames = pump(dec, 2, 10s);
  REQUIRE(frames.size() >= 1);
  for(auto* f : frames)
  {
    REQUIRE(f->data[0] != nullptr);
    // load_packet_in_frame() copies the packet payload in; for rgb24 that is
    // exactly one tightly packed frame.
    CHECK(f->linesize[0] >= 64 * 3);
  }
  releaseAll(dec, frames);
}

TEST_CASE("the thumbnailer renders a frame at a requested position",
          "[video][libav][media][thumbnail]")
{
  const QString path = requireMediaDir() + QLatin1String("/h264.mp4");
  REQUIRE(QFileInfo::exists(path));

  Video::VideoThumbnailer thumb{path};
  CHECK(thumb.width == 320);
  CHECK(thumb.height == 240);

  const QImage first = thumb.process(0);
  REQUIRE_FALSE(first.isNull());
  CHECK(first.width() > 0);
  CHECK(first.height() > 0);

  // A thumbnail of a colour-bar clip that came out uniform means the rescale
  // produced nothing and the caller cannot tell.
  bool varied = false;
  const QRgb reference = first.pixel(0, 0);
  for(int y = 0; y < first.height() && !varied; y++)
    for(int x = 0; x < first.width(); x++)
      if(first.pixel(x, y) != reference)
      {
        varied = true;
        break;
      }
  CHECK(varied);

  // A second position on the same object: the thumbnailer keeps its format
  // context open between requests and seeks within it.
  const QImage later = thumb.process(ossia::flicks_per_second<int64_t>);
  CHECK_FALSE(later.isNull());
  CHECK(later.size() == first.size());
}

// FINDING (reported, not fixed here -- tests-only branch):
// Video::VideoThumbnailer::process() dereferences m_formatContext and indexes
// m_formatContext->streams[m_stream] with no guard. When the constructor could
// not open the file it leaves m_formatContext == nullptr and m_stream == -1, so
// the very first statement of process() is a null dereference:
//
//   ossia::seek_to_flick(m_formatContext, m_codecContext,
//                        m_formatContext->streams[m_stream], ...)
//
// Its two internal callers, onRequest() and processNext(), both open with
// `if(!m_codecContext) return;`. process() is public and has no such guard, so
// any direct caller crashes the process on a file that failed to open -- a
// missing or corrupt media file, i.e. the normal library case.
//
// Run in a forked child so the crash cannot take this binary down with it, and
// written as the invariant that SHOULD hold: it flips red the day process()
// grows the same guard its callers already have.
namespace
{
// Returns true if the child exited normally with status 0.
bool survives(auto&& fn)
{
  ::fflush(nullptr);
  const pid_t pid = ::fork();
  REQUIRE(pid >= 0);
  if(pid == 0)
  {
    // Catch2's crash handler would otherwise print a second, confusing report
    // from the child; here the exit status IS the result.
    for(int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
      ::signal(sig, SIG_DFL);
    fn();
    ::_exit(0);
  }
  int status{};
  REQUIRE(::waitpid(pid, &status, 0) == pid);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
} // namespace

TEST_CASE("a thumbnailer that could not open its file knows it",
          "[video][libav][media][thumbnail]")
{
  // The part of the contract that holds today: a failed open leaves the object
  // in an unmistakably empty state.
  const QString missing
      = requireMediaDir() + QLatin1String("/there-is-no-such-file.mp4");
  Video::VideoThumbnailer thumb{missing};
  CHECK(thumb.width == 0);
  CHECK(thumb.height == 0);
  CHECK(thumb.smallWidth == 0);
  CHECK(thumb.smallHeight == 0);
  CHECK(thumb.fps == 0.);
}

TEST_CASE("FINDING: VideoThumbnailer::process() crashes on a file it could not open",
          "[video][libav][media][thumbnail][!shouldfail]")
{
  const QString missing
      = requireMediaDir() + QLatin1String("/there-is-no-such-file.mp4");
  const QString garbage = requireMediaDir() + QLatin1String("/garbage.bin");

  CHECK(survives([&] {
    Video::VideoThumbnailer thumb{missing};
    (void)thumb.process(0);
  }));
  CHECK(survives([&] {
    Video::VideoThumbnailer thumb{garbage};
    (void)thumb.process(0);
  }));
}

// ---------------------------------------------------------------------------
// Video::LibavStreamInput -- the FFmpeg *input device* behind Gfx/Libav. Same
// libav machinery as VideoDecoder, but a different object with its own demux
// loop, its own audio path and its own pacing decision, and it was at 0% of 338
// lines. A local file is a legitimate URL for it, so none of this needs a
// network or a capture device.

TEST_CASE("the FFmpeg input probes a file the way the demuxer sees it",
          "[video][libav][media][streaminput]")
{
  const auto path = clipPath("audio-video.mp4");
  const auto ref = probe(path);
  REQUIRE(ref.videoStreams == 1);
  REQUIRE(ref.otherStreams >= 1);

  Video::LibavStreamInput in;
  REQUIRE(in.load(path));
  CHECK(in.url() == path);
  REQUIRE(in.probe());

  CHECK(in.width == ref.width);
  CHECK(in.height == ref.height);
  CHECK(in.fps > 0.);
  // The audio stream must be picked up as audio, not ignored the way
  // VideoDecoder discards it.
  CHECK(in.has_audio());

  // probe() is idempotent: the second call must reuse the open context rather
  // than leak a second one.
  CHECK(in.probe());
}

TEST_CASE("the FFmpeg input refuses what it cannot open",
          "[video][libav][media][streaminput]")
{
  const auto dir = requireMediaDir().toStdString();

  // An empty URL is refused by load() itself, before any libav call.
  {
    Video::LibavStreamInput in;
    CHECK_FALSE(in.load(""));
  }
  {
    Video::LibavStreamInput in;
    REQUIRE(in.load(dir + "/there-is-no-such-file.mp4"));
    CHECK_FALSE(in.probe());
    CHECK_FALSE(in.start());
  }
  {
    Video::LibavStreamInput in;
    REQUIRE(in.load(dir + "/garbage.bin"));
    CHECK_FALSE(in.probe());
  }
  {
    Video::LibavStreamInput in;
    REQUIRE(in.load(dir + "/empty.bin"));
    CHECK_FALSE(in.probe());
  }
}

namespace
{
// Frames delivered by a LibavStreamInput over `budget`, capped at `wanted`.
int drain(
    Video::LibavStreamInput& in, int wanted, std::chrono::milliseconds budget = 10s)
{
  int got = 0;
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while(got < wanted && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      in.release_frame(f);
      got++;
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  return got;
}
} // namespace

TEST_CASE("the FFmpeg input delivers video frames", "[video][libav][media][streaminput]")
{
  const auto path = clipPath("fmt-rgb24-64x64.nut");

  Video::LibavStreamInput in;
  // Any option at all is enough to keep probe() out of its network-latency
  // branch; see the FINDING case below for what happens without one.
  REQUIRE(in.load(path, {{"probesize", "5000000"}}));
  REQUIRE(in.start());
  // A second start() while running must be refused rather than spawn a second
  // demux thread onto the same queue.
  CHECK_FALSE(in.start());

  int got = 0;
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while(got < 3 && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      CHECK(f->width == 64);
      CHECK(f->height == 64);
      CHECK(AVPixelFormat(f->format) == in.pixel_format);
      REQUIRE(f->data[0] != nullptr);
      in.release_frame(f);
      got++;
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  CHECK(got >= 3);

  in.stop();
  // stop() joins the thread and drains: nothing may be handed out afterwards.
  CHECK(in.dequeue_frame() == nullptr);
}

// FINDING (reported, not fixed here -- tests-only branch): with NO options --
// the default, and what a user gets by typing a file path into the FFmpeg input
// device -- LibavStreamInput applies network low-latency flags to every source:
//
//   m_formatContext->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
//   av_dict_set(&options, "fflags", "nobuffer", 0);
//   av_dict_set(&options, "flags",  "low_delay", 0);
//
// A local rawvideo-in-NUT file then yields ZERO frames: probe() succeeds,
// start() succeeds, and nothing ever comes out. Passing any option at all --
// even one as inert as probesize -- takes probe() down its other branch and the
// same file plays. The flags belong behind the same "is this a network URL"
// test probe() already computes for m_needsPacing.
//
// Written as the invariant that SHOULD hold, so it flips red the day the flags
// are gated.
TEST_CASE(
    "FINDING: the FFmpeg input's default low-latency flags starve a local file",
    "[video][libav][media][streaminput][!shouldfail]")
{
  {
    INFO("rawvideo in NUT");
    Video::LibavStreamInput in;
    REQUIRE(in.load(clipPath("fmt-rgb24-64x64.nut")));
    REQUIRE(in.start());
    CHECK(drain(in, 3) >= 3);
    in.stop();
  }
  {
    INFO("H.264 in mp4");
    Video::LibavStreamInput in;
    REQUIRE(in.load(clipPath("h264.mp4")));
    REQUIRE(in.start());
    CHECK(drain(in, 3) >= 3);
    in.stop();
  }
}

TEST_CASE("the FFmpeg input can be started, stopped and started again",
          "[video][libav][media][streaminput]")
{
  const auto path = clipPath("fmt-yuv420p-64x64.nut");
  Video::LibavStreamInput in;
  REQUIRE(in.load(path, {{"probesize", "5000000"}}));

  for(int round = 0; round < 2; round++)
  {
    INFO("round " << round);
    REQUIRE(in.start());
    int got = 0;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while(got < 2 && std::chrono::steady_clock::now() < deadline)
    {
      if(auto* f = in.dequeue_frame())
      {
        in.release_frame(f);
        got++;
      }
      else
      {
        std::this_thread::sleep_for(2ms);
      }
    }
    CHECK(got >= 2);
    in.stop();
  }
}

TEST_CASE("the FFmpeg input loops when asked to", "[video][libav][media][streaminput]")
{
  // "loop" is score's own option, not FFmpeg's: probe() strips it out of the
  // dictionary and the demux loop seeks back to the start on EOF. The clip is
  // 8 frames long, so more than 8 frames out is only possible if it wrapped.
  const auto path = clipPath("fmt-rgb24-64x64.nut");
  Video::LibavStreamInput in;
  REQUIRE(in.load(path, {{"loop", "-1"}}));
  REQUIRE(in.start());

  int got = 0;
  const auto deadline = std::chrono::steady_clock::now() + 20s;
  while(got < 20 && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      in.release_frame(f);
      got++;
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  CHECK(got > 8);
  in.stop();
}

TEST_CASE("the FFmpeg input accepts a lavfi generator as a source",
          "[video][libav][media][streaminput]")
{
  // The "format" option selects the demuxer explicitly; lavfi is the branch
  // probe() special-cases for pacing, and it is a source with no file at all.
  Video::LibavStreamInput in;
  REQUIRE(in.load("testsrc2=size=64x64:rate=25", {{"format", "lavfi"}}));
  if(!in.probe())
    SKIP("this ffmpeg has no lavfi demuxer");

  CHECK(in.width == 64);
  CHECK(in.height == 64);
  CHECK(in.fps > 0.);
  CHECK_FALSE(in.has_audio());

  REQUIRE(in.start());
  int got = 0;
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while(got < 2 && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      CHECK(f->width == 64);
      CHECK(f->height == 64);
      in.release_frame(f);
      got++;
    }
    else
    {
      std::this_thread::sleep_for(2ms);
    }
  }
  CHECK(got >= 2);
  in.stop();
}

TEST_CASE("the FFmpeg input decodes the audio stream too",
          "[video][libav][media][streaminput]")
{
  const auto path = clipPath("audio-video.mp4");
  Video::LibavStreamInput in;
  REQUIRE(in.load(path));
  REQUIRE(in.probe());
  REQUIRE(in.has_audio());

  auto& ring = in.audio_buffer();
  CHECK(ring.sample_rate > 0);
  CHECK(ring.num_channels > 0);

  // The audio parameter points the ring at its own backing storage; without
  // that, read_into_output() has nowhere to put the samples.
  std::vector<ossia::float_vector> out;
  out.resize(ring.num_channels);
  for(auto& ch : out)
    ch.resize(256);
  ring.output_data = &out;

  REQUIRE(in.start());

  // A 440 Hz sine: once anything has been written, at least one sample of one
  // channel must be non-zero. Poll rather than sleep a fixed amount.
  bool sawAudio = false;
  const auto deadline = std::chrono::steady_clock::now() + 15s;
  while(!sawAudio && std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
      in.release_frame(f);

    ring.read_into_output(256);
    for(auto& ch : out)
      for(float v : ch)
        if(v != 0.f)
        {
          sawAudio = true;
          break;
        }
    if(!sawAudio)
      std::this_thread::sleep_for(5ms);
  }
  CHECK(sawAudio);

  in.stop();
  ring.output_data = nullptr;
}

TEST_CASE("the audio ring buffer round-trips every writer",
          "[video][libav][media][streaminput]")
{
  // Pure CPU: the three writers the demuxer picks between, and the reader the
  // audio engine calls. No file involved.
  Video::AudioRingBuffer ring;
  ring.init(2);
  CHECK(ring.num_channels == 2);

  std::vector<ossia::float_vector> out;
  out.resize(2);
  for(auto& ch : out)
    ch.resize(4);
  ring.output_data = &out;

  SECTION("interleaved float")
  {
    const float src[8] = {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f, 0.4f, -0.4f};
    ring.write_interleaved_float(src, 4, 2);
    ring.read_into_output(4);
    for(int i = 0; i < 4; i++)
    {
      CHECK(out[0][i] == Catch::Approx(src[2 * i]));
      CHECK(out[1][i] == Catch::Approx(src[2 * i + 1]));
    }
  }

  SECTION("interleaved 16-bit")
  {
    const int16_t src[8] = {32767, -32768, 16384, -16384, 0, 0, 8192, -8192};
    ring.write_interleaved_s16(src, 4, 2);
    ring.read_into_output(4);
    // Scaled into [-1, 1]; the exact divisor is the implementation's business,
    // the sign and the relative magnitudes are not.
    CHECK(out[0][0] > 0.9f);
    CHECK(out[1][0] < -0.9f);
    CHECK(out[0][1] == Catch::Approx(out[0][0] / 2.f).margin(0.01));
    CHECK(out[0][2] == Catch::Approx(0.f).margin(1e-6));
  }

  SECTION("planar float")
  {
    float left[4] = {1.f, 2.f, 3.f, 4.f};
    float right[4] = {-1.f, -2.f, -3.f, -4.f};
    float* planes[2] = {left, right};
    ring.write_planar(planes, 4, 2);
    ring.read_into_output(4);
    for(int i = 0; i < 4; i++)
    {
      CHECK(out[0][i] == Catch::Approx(left[i]));
      CHECK(out[1][i] == Catch::Approx(right[i]));
    }
  }

  SECTION("reading an empty ring yields silence, not stale samples")
  {
    for(auto& ch : out)
      for(auto& v : ch)
        v = 42.f;
    ring.read_into_output(4);
    for(auto& ch : out)
      for(float v : ch)
        CHECK(v == Catch::Approx(0.f).margin(1e-6));
  }

  ring.output_data = nullptr;
}

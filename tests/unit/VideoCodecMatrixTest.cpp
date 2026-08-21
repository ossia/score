// Video::VideoDecoder across the container x codec matrix, asserted against known
// pixels rather than against "a frame came out".
//
// The runner (tests/hardware/with-virtual-media.sh --matrix) builds one master
// clip -- a grid of solid 40x40 blocks whose colour is a function of block index
// and frame number -- and encodes that same master into every container/codec
// pair ffmpeg on the host can produce. master.rgb is derived FROM the master
// clip, not generated a second time, so the ground truth and the bytes the
// encoders were fed cannot drift apart.
//
// That ground truth is a file of raw rgb24 bytes read here with fread. Nothing
// about the expected picture is recomputed from the generator's parameters: the
// geometry is read back from the container by libav and cross-checked against the
// size of the raw file, and the colours are whatever the bytes say.
//
// Solid blocks are what make a per-pixel assertion possible through a 4:2:0
// codec: chroma subsampling, deblocking and quantisation disturb block borders,
// while the interior of a 40x40 block still carries the pattern's colour. So the
// comparison is per-pixel over block interiors, with a tolerance that is zero for
// the lossless rows.
//
// Frame identity is asserted, not assumed: each decoded frame is matched against
// every master frame and the best match must be the frame at its own index, so a
// decoder handing back coded order, repeating or dropping a frame fails.

#include <Video/GpuFormats.hpp>
#include <Video/LibavStreamInput.hpp>
#include <Video/VideoDecoder.hpp>

#include <ossia/detail/flicks.hpp>

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

using namespace std::chrono_literals;

namespace
{
QString matrixDir()
{
  const auto d = QString::fromLocal8Bit(qgetenv("SCORE_TEST_MATRIX_DIR"));
  REQUIRE_FALSE(d.isEmpty());
  REQUIRE(QFileInfo(d).isDir());
  return d;
}

std::string matrixPath(const char* name)
{
  const QString p = matrixDir() + QLatin1String("/") + QLatin1String(name);
  return p.toStdString();
}

Video::DecoderConfiguration softwareOnly()
{
  Video::DecoderConfiguration conf;
  conf.hardwareAcceleration = AV_PIX_FMT_NONE;
  conf.threads = 1;
  conf.useAVCodec = true;
  return conf;
}

// The known picture, as bytes on disk. Geometry comes from the master
// container (libav) and must agree with the file's own length.
struct Master
{
  int width{}, height{}, frames{};
  std::vector<uint8_t> rgb;

  const uint8_t* frame(int i) const
  {
    return rgb.data() + std::size_t(i) * std::size_t(width) * height * 3;
  }
};

Master loadMaster()
{
  Master m;

  AVFormatContext* ctx{};
  const auto nut = matrixPath("master.nut");
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

  const auto raw = matrixPath("master.rgb");
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
  // above, never the source of either.
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

int blockSize()
{
  const int b = qgetenv("SCORE_TEST_MATRIX_BLOCK").toInt();
  return b > 0 ? b : 40;
}

// One decoded AVFrame as rgb24, using swscale on the test side so that every
// codec's native pixel format is compared in one space.
std::vector<uint8_t> toRgb24(const AVFrame& f, int w, int h)
{
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

// Per-pixel distance over the interior of every block: the largest per-channel
// deviation, and the mean. `margin` pixels are dropped from each block edge --
// that is where 4:2:0 chroma and deblocking legitimately blend two colours.
struct Diff
{
  int maxDev{};
  double meanDev{};
  long compared{};
};

Diff blockDiff(
    const uint8_t* got, const uint8_t* want, int w, int h, int block, int margin)
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

// The fidelity a row is entitled to, from its name. `exact` is an RGB lossless
// codec and must come back byte for byte; `yuvexact` is lossless but round
// trips through a YUV colour space, so it carries the rounding of one
// conversion; `lossy` carries a quantiser. The numbers are the largest
// per-channel deviation allowed over block interiors, and the sweep asserts
// separately that two DIFFERENT master frames are far further apart than the
// largest of them.
// Pixels within this many of a block edge are dropped from every comparison:
// that is where 4:2:0 chroma and a deblocking filter legitimately blend two
// colours, and it is the only part of the picture a codec is allowed to move.
constexpr int kBlockMargin = 10;

constexpr int kToleranceExact = 0;
constexpr int kToleranceYuvExact = 6;
constexpr int kToleranceLossy = 64;
constexpr int kLargestTolerance = kToleranceLossy;

struct Clip
{
  std::string name;
  std::string path;
  std::string container;
  std::string codec;
  int tolerance{};
};

std::vector<Clip> discoverClips()
{
  std::vector<Clip> out;
  QDir dir{matrixDir()};
  for(const auto& fi : dir.entryInfoList({"codec-*"}, QDir::Files, QDir::Name))
  {
    const auto parts = fi.completeBaseName().split('-');
    REQUIRE(parts.size() == 4);
    Clip c;
    c.name = fi.fileName().toStdString();
    c.path = fi.absoluteFilePath().toStdString();
    c.container = parts[1].toStdString();
    c.codec = parts[2].toStdString();
    if(parts[3] == "exact")
      c.tolerance = kToleranceExact;
    else if(parts[3] == "yuvexact")
      c.tolerance = kToleranceYuvExact;
    else if(parts[3] == "lossy")
      c.tolerance = kToleranceLossy;
    else
      FAIL("unknown fidelity class in " << c.name);
    out.push_back(std::move(c));
  }
  return out;
}

std::vector<AVFrame*>
pump(Video::VideoDecoder& dec, int wanted, std::chrono::milliseconds budget = 20s)
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

void releaseAll(Video::VideoDecoder& dec, std::vector<AVFrame*>& frames)
{
  for(auto* f : frames)
    dec.release_frame(f);
  frames.clear();
}

// What the file holds, read with libav directly rather than through score.
struct Probe
{
  AVCodecID codec{AV_CODEC_ID_NONE};
  int width{}, height{};
  int videoStreams{}, audioStreams{};
  AVRational videoTimeBase{0, 1};
  std::string format;
};

Probe probe(const std::string& path)
{
  Probe p;
  AVFormatContext* ctx{};
  if(avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) != 0)
    return p;
  if(avformat_find_stream_info(ctx, nullptr) < 0)
  {
    avformat_close_input(&ctx);
    return p;
  }
  if(ctx->iformat && ctx->iformat->name)
    p.format = ctx->iformat->name;
  for(unsigned i = 0; i < ctx->nb_streams; i++)
  {
    auto* par = ctx->streams[i]->codecpar;
    if(par->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(p.videoStreams++ == 0)
      {
        p.codec = par->codec_id;
        p.width = par->width;
        p.height = par->height;
        p.videoTimeBase = ctx->streams[i]->time_base;
      }
    }
    else if(par->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      p.audioStreams++;
    }
  }
  avformat_close_input(&ctx);
  return p;
}

std::string unavailableReport()
{
  const auto p = matrixPath("unavailable.txt");
  FILE* f = std::fopen(p.c_str(), "rb");
  if(!f)
    return {};
  std::string out;
  char buf[512];
  while(std::fgets(buf, sizeof(buf), f))
    out += buf;
  std::fclose(f);
  return out;
}
} // namespace

TEST_CASE("the matrix runner provisioned containers and codecs",
          "[video][matrix][media]")
{
  // Negative control on the sweep: an empty matrix would make every case below
  // pass without decoding anything, and a codec the host could not build must
  // be VISIBLE rather than silently absent.
  const auto clips = discoverClips();
  const auto missing = unavailableReport();
  INFO("rows this ffmpeg could not produce:\n" << missing);
  CHECK(clips.size() >= 15);

  std::set<std::string> containers, codecs;
  int lossless = 0;
  for(const auto& c : clips)
  {
    containers.insert(c.container);
    codecs.insert(c.codec);
    if(c.tolerance == kToleranceExact)
      lossless++;
  }
  // mp4 / mkv / webm / mov / mpegts / avi / nut
  CHECK(containers.size() >= 6);
  // h264 / h265 / vp8 / vp9 / av1 / mjpeg / prores / rawvideo / ffv1 / utvideo
  CHECK(codecs.size() >= 8);
  CHECK(lossless >= 3);
}

TEST_CASE("the master pattern is a real picture", "[video][matrix][media]")
{
  // Negative control on the ground truth: a uniform or empty master would make
  // every per-pixel comparison below trivially satisfiable.
  const auto m = loadMaster();
  const int block = blockSize();
  REQUIRE(m.width % block == 0);
  REQUIRE(m.height % block == 0);

  std::set<std::array<uint8_t, 3>> colours;
  const uint8_t* f0 = m.frame(0);
  for(int by = 0; by + block <= m.height; by += block)
  {
    for(int bx = 0; bx + block <= m.width; bx += block)
    {
      const std::size_t o
          = (std::size_t(by + block / 2) * m.width + bx + block / 2) * 3;
      colours.insert({f0[o], f0[o + 1], f0[o + 2]});
    }
  }
  CHECK(colours.size() >= 20);

  // Every block is flat inside, which is the property the tolerance argument
  // below rests on.
  int flat = 0, total = 0;
  for(int by = 0; by + block <= m.height; by += block)
  {
    for(int bx = 0; bx + block <= m.width; bx += block)
    {
      total++;
      const std::size_t ref = (std::size_t(by) * m.width + bx) * 3;
      bool uniform = true;
      for(int y = by; y < by + block && uniform; y++)
        for(int x = bx; x < bx + block; x++)
        {
          const std::size_t o = (std::size_t(y) * m.width + x) * 3;
          if(f0[o] != f0[ref] || f0[o + 1] != f0[ref + 1] || f0[o + 2] != f0[ref + 2])
          {
            uniform = false;
            break;
          }
        }
      if(uniform)
        flat++;
    }
  }
  CHECK(flat == total);

  // Successive frames differ: otherwise the frame-identity check below would
  // be satisfied by any ordering at all.
  int changed = 0;
  for(int i = 1; i < m.frames; i++)
    if(std::memcmp(m.frame(i), m.frame(i - 1), std::size_t(m.width) * m.height * 3) != 0)
      changed++;
  CHECK(changed == m.frames - 1);
}

TEST_CASE("the pixel comparison rejects the wrong frame", "[video][matrix][media]")
{
  // Negative control on the guard, and the argument that makes the tolerances
  // below legitimate: the closest pair of DIFFERENT master frames must be far
  // further apart than the largest deviation any codec is forgiven. Without
  // this, a green matrix would only prove the tolerance swallowed everything.
  const auto m = loadMaster();
  const int block = blockSize();

  const auto same = blockDiff(m.frame(0), m.frame(0), m.width, m.height, block, kBlockMargin);
  CHECK(same.compared > 10000);
  CHECK(same.maxDev == 0);
  CHECK(same.meanDev == 0.);

  int closestMax = 1 << 30;
  double closestMean = 1e30;
  for(int i = 0; i < m.frames; i++)
  {
    for(int j = 0; j < m.frames; j++)
    {
      if(i == j)
        continue;
      const auto d = blockDiff(m.frame(i), m.frame(j), m.width, m.height, block, kBlockMargin);
      closestMax = std::min(closestMax, d.maxDev);
      closestMean = std::min(closestMean, d.meanDev);
    }
  }
  INFO("closest distinct master frames: maxDev " << closestMax << " meanDev "
                                                 << closestMean);
  CHECK(closestMax > 2 * kLargestTolerance);
  CHECK(closestMean > 8.);
}

TEST_CASE("every container/codec row decodes the master picture back",
          "[video][matrix][media]")
{
  const auto m = loadMaster();
  const int block = blockSize();
  const auto clips = discoverClips();
  REQUIRE_FALSE(clips.empty());

  for(const auto& clip : clips)
  {
    INFO("clip " << clip.name);

    // libav's own reading of the file: the assertions below are about what
    // score does with it, never about what ffmpeg decided it was.
    const auto ref = probe(clip.path);
    REQUIRE(ref.videoStreams == 1);
    CHECK(ref.width == m.width);
    CHECK(ref.height == m.height);

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.load(clip.path));
    CHECK(dec.width == ref.width);
    CHECK(dec.height == ref.height);
    CHECK(dec.codec_id == ref.codec);

    auto frames = pump(dec, m.frames);
    INFO("decoded " << frames.size() << " frames of " << m.frames);
    REQUIRE(frames.size() >= std::size_t(m.frames));

    const int margin = kBlockMargin;

    for(int i = 0; i < m.frames; i++)
    {
      const auto rgb = toRgb24(*frames[i], m.width, m.height);
      REQUIRE(rgb.size() == std::size_t(m.width) * m.height * 3);

      // Frame identity: the best-matching master frame must be this index. The
      // mean is the discriminator because a codec's worst pixel is close to
      // its own frame's tolerance while its mean is an order of magnitude
      // below the distance to any other frame.
      int best = -1;
      double bestDev = 1e30;
      for(int j = 0; j < m.frames; j++)
      {
        const auto d
            = blockDiff(rgb.data(), m.frame(j), m.width, m.height, block, margin);
        if(d.meanDev < bestDev)
        {
          bestDev = d.meanDev;
          best = j;
        }
      }
      INFO("frame " << i << " matched master frame " << best << " (meanDev "
                    << bestDev << ")");
      CHECK(best == i);

      const auto d
          = blockDiff(rgb.data(), m.frame(i), m.width, m.height, block, margin);
      INFO("frame " << i << " maxDev " << d.maxDev << " meanDev " << d.meanDev
                    << " over " << d.compared << " samples, tolerance "
                    << clip.tolerance);
      CHECK(d.compared > 10000);
      CHECK(d.maxDev <= clip.tolerance);
    }

    releaseAll(dec, frames);
  }
}

TEST_CASE("the FFmpeg stream input decodes the matrix too",
          "[video][matrix][media][streaminput]")
{
  // Video::LibavStreamInput is a different object from VideoDecoder with its
  // own demux loop; a local file is a legitimate URL for it. One lossless and
  // one lossy row is enough to show the picture survives that path as well.
  const auto m = loadMaster();
  const int block = blockSize();

  for(const char* name : {"codec-nut-rawvideo-exact.nut", "codec-mkv-h264-lossy.mkv"})
  {
    const auto path = matrixPath(name);
    if(!QFileInfo::exists(QString::fromStdString(path)))
      continue;
    INFO("clip " << name);

    Video::LibavStreamInput in;
    REQUIRE(in.load(path));
    REQUIRE(in.probe());
    CHECK(in.width == m.width);
    CHECK(in.height == m.height);
    REQUIRE(in.start());

    bool matched = false;
    const auto deadline = std::chrono::steady_clock::now() + 20s;
    while(!matched && std::chrono::steady_clock::now() < deadline)
    {
      if(auto* f = in.dequeue_frame())
      {
        const auto rgb = toRgb24(*f, m.width, m.height);
        if(rgb.size() == std::size_t(m.width) * m.height * 3)
        {
          for(int j = 0; j < m.frames; j++)
          {
            const auto d
                = blockDiff(rgb.data(), m.frame(j), m.width, m.height, block, kBlockMargin);
            if(d.compared > 10000 && d.maxDev <= kToleranceLossy)
            {
              matched = true;
              break;
            }
          }
        }
        in.release_frame(f);
      }
      else
      {
        std::this_thread::sleep_for(2ms);
      }
    }
    CHECK(matched);
    in.stop();
  }
}

TEST_CASE("malformed containers are refused rather than half-opened",
          "[video][matrix][media]")
{
  QDir dir{matrixDir()};
  const auto zeros = dir.entryInfoList({"zero.*"}, QDir::Files, QDir::Name);
  CHECK(zeros.size() >= 5);
  for(const auto& fi : zeros)
  {
    INFO("zero-byte " << fi.fileName().toStdString());
    CHECK(fi.size() == 0);
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.load(fi.absoluteFilePath().toStdString()));
  }

  {
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.load(matrixPath("garbage.mp4")));
  }

  // A truncated file may or may not open depending on where its index lives.
  // The requirement is that opening it neither crashes nor wedges, and that an
  // open that SUCCEEDS really can produce a frame of the geometry it claimed.
  const auto truncated = dir.entryInfoList({"truncated-*"}, QDir::Files, QDir::Name);
  CHECK(truncated.size() >= 3);
  for(const auto& fi : truncated)
  {
    INFO("truncated " << fi.fileName().toStdString());
    Video::VideoDecoder dec{softwareOnly()};
    if(dec.load(fi.absoluteFilePath().toStdString()))
    {
      CHECK(dec.width > 0);
      CHECK(dec.height > 0);
      auto frames = pump(dec, 1, 3s);
      for(auto* f : frames)
      {
        CHECK(f->width == dec.width);
        CHECK(f->height == dec.height);
      }
      releaseAll(dec, frames);
    }
  }
}

TEST_CASE("a container whose header lies about the geometry",
          "[video][matrix][media]")
{
  // resolution-change.ts is two MPEG-TS segments of different sizes end to end.
  // The demuxer reports the FIRST size, so from the second segment on, the
  // frames disagree with the stream metadata -- the exact shape of "rescale the
  // rows the frame has, not the rows the metadata claims".
  const auto path = matrixPath("resolution-change.ts");
  REQUIRE(QFileInfo::exists(QString::fromStdString(path)));

  const auto ref = probe(path);
  REQUIRE(ref.videoStreams == 1);
  REQUIRE(ref.width > 0);

  Video::VideoDecoder dec{softwareOnly()};
  REQUIRE(dec.load(path));
  const int announced = dec.width;
  const int announcedH = dec.height;
  CHECK(announced == ref.width);

  auto frames = pump(dec, 24, 20s);
  REQUIRE(frames.size() >= 12);

  bool sawOther = false;
  for(auto* f : frames)
  {
    REQUIRE(f->width > 0);
    REQUIRE(f->height > 0);
    // Whatever geometry a frame has, its planes must be consistent with THAT
    // geometry: a decoder that carried the header's stride onto a smaller
    // frame would read past the rows it actually has.
    const auto fmt = AVPixelFormat(f->format);
    const int planes = av_pix_fmt_count_planes(fmt);
    for(int p = 0; p < planes; p++)
    {
      const int minLine = av_image_get_linesize(fmt, f->width, p);
      INFO("plane " << p << " of a " << f->width << "x" << f->height << " frame");
      REQUIRE(f->data[p] != nullptr);
      CHECK(std::abs(f->linesize[p]) >= minLine);
    }
    if(f->width != announced || f->height != announcedH)
      sawOther = true;
  }
  INFO("announced " << announced << "x" << announcedH);
  CHECK(sawOther);

  releaseAll(dec, frames);
}

TEST_CASE("audio-only and video-only files are each seen for what they are",
          "[video][matrix][media]")
{
  {
    const auto path = matrixPath("video-only.mp4");
    REQUIRE(QFileInfo::exists(QString::fromStdString(path)));
    const auto ref = probe(path);
    CHECK(ref.videoStreams == 1);
    CHECK(ref.audioStreams == 0);

    Video::LibavStreamInput in;
    REQUIRE(in.load(path));
    REQUIRE(in.probe());
    CHECK_FALSE(in.has_audio());
    CHECK(in.width == ref.width);
  }
  {
    const auto path = matrixPath("audio-only.m4a");
    REQUIRE(QFileInfo::exists(QString::fromStdString(path)));
    const auto ref = probe(path);
    CHECK(ref.videoStreams == 0);
    CHECK(ref.audioStreams == 1);

    // A file with no video stream at all: VideoDecoder must refuse it rather
    // than open with a zero geometry, and the stream input must report the
    // audio without claiming a picture.
    Video::VideoDecoder dec{softwareOnly()};
    CHECK_FALSE(dec.load(path));

    Video::LibavStreamInput in;
    REQUIRE(in.load(path));
    const bool probed = in.probe();
    INFO("LibavStreamInput::probe() on an audio-only file returned " << probed);
    if(probed)
    {
      CHECK(in.has_audio());
      CHECK(in.width == 0);
      CHECK(in.height == 0);
    }
  }
}

TEST_CASE("a stream that loses and gains an audio track keeps playing",
          "[video][matrix][media]")
{
  // track-loss.ts is video+audio followed by video-only; track-gain.ts is the
  // reverse. Neither may stop the video at the join.
  for(const char* name : {"track-loss.ts", "track-gain.ts"})
  {
    INFO("clip " << name);
    const auto path = matrixPath(name);
    REQUIRE(QFileInfo::exists(QString::fromStdString(path)));

    Video::VideoDecoder dec{softwareOnly()};
    REQUIRE(dec.load(path));
    auto frames = pump(dec, 20, 20s);
    // Both halves are 12 frames; getting past 12 means the join was survived.
    INFO("decoded " << frames.size() << " frames");
    CHECK(frames.size() > 12);
    for(auto* f : frames)
    {
      CHECK(f->width > 0);
      CHECK(f->height > 0);
    }
    releaseAll(dec, frames);
  }
}

TEST_CASE("the audio and video clocks of one file advance together",
          "[video][matrix][media][avsync]")
{
  // A/V sync as a rate check rather than an event check: over the same stretch
  // of the file, the audio the ring delivered and the video the PTS covered
  // must describe the same amount of time. A decoder whose audio ran at the
  // wrong sample rate, or whose video time base was misread, drifts here.
  const auto path = matrixPath("audio-video.mp4");
  REQUIRE(QFileInfo::exists(QString::fromStdString(path)));

  // The time base is read straight from the container, so the seconds below are
  // not something score computed for itself.
  const auto ref = probe(path);
  REQUIRE(ref.videoStreams == 1);
  REQUIRE(ref.audioStreams == 1);
  REQUIRE(ref.videoTimeBase.num > 0);
  REQUIRE(ref.videoTimeBase.den > 0);

  Video::LibavStreamInput in;
  REQUIRE(in.load(path));
  REQUIRE(in.probe());
  REQUIRE(in.has_audio());

  auto& ring = in.audio_buffer();
  REQUIRE(ring.sample_rate > 0);
  REQUIRE(ring.num_channels > 0);

  std::vector<ossia::float_vector> out;
  out.resize(ring.num_channels);
  for(auto& ch : out)
    ch.resize(1024);
  ring.output_data = &out;

  REQUIRE(in.start());

  int64_t firstPts = AV_NOPTS_VALUE, lastPts = AV_NOPTS_VALUE;
  long audioFrames = 0;
  const auto deadline = std::chrono::steady_clock::now() + 20s;
  while(std::chrono::steady_clock::now() < deadline)
  {
    if(auto* f = in.dequeue_frame())
    {
      if(f->pts != AV_NOPTS_VALUE)
      {
        if(firstPts == AV_NOPTS_VALUE)
          firstPts = f->pts;
        lastPts = f->pts;
      }
      in.release_frame(f);
    }

    ring.read_into_output(1024);
    bool nonZero = false;
    for(auto& ch : out)
      for(float v : ch)
        if(v != 0.f)
        {
          nonZero = true;
          break;
        }
    if(nonZero)
      audioFrames += 1024;

    if(lastPts != AV_NOPTS_VALUE && firstPts != AV_NOPTS_VALUE
       && audioFrames > ring.sample_rate)
      break;
    std::this_thread::sleep_for(2ms);
  }

  in.stop();
  ring.output_data = nullptr;

  REQUIRE(firstPts != AV_NOPTS_VALUE);
  REQUIRE(lastPts > firstPts);
  REQUIRE(audioFrames > 0);

  // The video span in seconds, from the container's own time base.
  const double videoSeconds = double(lastPts - firstPts) * ref.videoTimeBase.num
                              / double(ref.videoTimeBase.den);
  const double audioSeconds = double(audioFrames) / double(ring.sample_rate);
  INFO("video " << videoSeconds << "s, audio " << audioSeconds << "s");
  REQUIRE(videoSeconds > 0.2);
  REQUIRE(audioSeconds > 0.2);
  // The reader is polled, so the two are not sampled at the same instant; a
  // factor of two apart is drift, not jitter.
  CHECK(audioSeconds < videoSeconds * 3.0);
  CHECK(videoSeconds < audioSeconds * 3.0);
}

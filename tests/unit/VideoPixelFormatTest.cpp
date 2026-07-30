// Unit tests for the video pixel-format vocabulary
// (Gfx/Graph/interop/VideoPixelFormat.{hpp,cpp}) and its libav bridge
// (VideoPixelFormatAV.{hpp,cpp}). Pure mapping logic, no app context.
//
// Every sweep iterates allFormats(), the same declarative table the vocabulary
// is generated from, so a new format is swept the moment it is declared and
// there is no second list to forget. A test keeping its own list of formats and
// asserting each has a descriptor is a tautology that cannot see a format
// declared in the enum but absent from the list.
//
// Covered for every format in the vocabulary:
//   - descriptor invariants: block geometry, plane count vs planarity,
//     subsampling factors, colour model, name
//   - sizing: tight rowBytes, alignment behaviour, hand-computed values
//     including odd widths and the v210 48-pixel/128-byte SMPTE rule
//   - bytesPerFrame against per-plane hand computation
//   - AV bridge round-trips both ways, wire-only formats mapping to
//     AV_PIX_FMT_NONE, unknown AV formats mapping to Unknown
//   - cross-validation of every mapped format against FFmpeg's own
//     av_pix_fmt_desc_get(): plane count, chroma subsampling, planarity,
//     alpha and RGB-ness. A mismatch there is a real bug, not a test artifact.

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>
#if defined(__linux__)
#include <Gfx/Graph/interop/V4L2PixelFormat.hpp>
#include <linux/videodev2.h>
#endif

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

namespace vpf = score::gfx::interop;
using V = vpf::VideoPixelFormat;
using vpf::ColorModel;

namespace
{
// The vocabulary, straight from the table that generates it.
std::vector<const vpf::VideoPixelFormatInfo*> described()
{
  std::size_t n = 0;
  const auto* all = vpf::allFormats(n);
  std::vector<const vpf::VideoPixelFormatInfo*> out;
  out.reserve(n);
  for(std::size_t i = 0; i < n; ++i)
    out.push_back(all + i);
  return out;
}

// A few widths that between them exercise exact blocks, partial blocks, odd
// values and the 8K rasters the SDI paths actually run.
constexpr uint32_t kWidths[] = {1, 2, 6, 63, 64, 65, 720, 1919, 1920, 1921, 3840, 7680, 8192};
} // namespace

// The values are serialized, so freezing a representative spread here makes an
// accidental renumbering break the build rather than silently invalidate saved
// documents.
static_assert(uint16_t(V::Unknown) == 0);
static_assert(uint16_t(V::BGRA8) == 1);
static_assert(uint16_t(V::BGRX8) == 7);
static_assert(uint16_t(V::XBGR8) == 19);
static_assert(uint16_t(V::UYVY422) == 20);
static_assert(uint16_t(V::V210) == 30);
static_assert(uint16_t(V::NV12) == 40);
static_assert(uint16_t(V::YVU420P) == 45);
static_assert(uint16_t(V::YUV422P12) == 55);
static_assert(uint16_t(V::YUV444P) == 60);
static_assert(uint16_t(V::YUVX) == 73);
static_assert(uint16_t(V::Mono8) == 80);
static_assert(uint16_t(V::Mono16BE) == 86);
static_assert(uint16_t(V::YUV565) == 100);

TEST_CASE("the vocabulary is non-trivial and self-consistent", "[gfx][pixfmt]")
{
  const auto all = described();
  REQUIRE(all.size() == vpf::formatCount());
  // Guards against the table being accidentally emptied or halved.
  CHECK(all.size() >= 69);

  for(const auto* i : all)
  {
    INFO("format " << i->name);
    // Every described format must be usable: the whole point of generating the
    // enum from the table is that this can no longer fail.
    CHECK(i->valid());
    CHECK(i->blockBytes > 0);
    CHECK(i->blockPixels >= 1);
    CHECK(i->name != nullptr);
    CHECK(std::string(i->name) != "unknown");
    CHECK(i->colorModel != ColorModel::Unknown);
    CHECK(i->planeCount >= 1);
    CHECK(i->planeCount <= 4);
    CHECK(i->isPlanar() == (i->planeCount > 1));
    // Subsampling is only meaningful in the two documented axes.
    CHECK((i->horizontalSubsampling == 1 || i->horizontalSubsampling == 2
           || i->horizontalSubsampling == 4));
    CHECK((i->verticalSubsampling == 1 || i->verticalSubsampling == 2
           || i->verticalSubsampling == 4));
    // formatInfo() must resolve the value back to this very row.
    CHECK(&vpf::formatInfo(i->format) == i);
    CHECK(std::string(vpf::formatName(i->format)) == i->name);
  }
}

TEST_CASE("names and values are unique across the vocabulary", "[gfx][pixfmt]")
{
  std::set<std::string> names;
  std::set<uint16_t> values;
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    CHECK(names.insert(i->name).second);
    CHECK(values.insert(uint16_t(i->format)).second);
    // Zero is the sentinel and must not be reused by a real format.
    CHECK(uint16_t(i->format) != 0);
  }
}

TEST_CASE("achromatic and chroma-bearing formats are classified", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    // Grey and Bayer carry no chroma, so they cannot be subsampled.
    if(i->isAchromatic())
    {
      CHECK(i->horizontalSubsampling == 1);
      CHECK(i->verticalSubsampling == 1);
      CHECK(i->planeCount == 1);
      CHECK_FALSE(i->isYuv());
      CHECK_FALSE(i->isRgb());
    }
    // RGB is never subsampled either.
    if(i->isRgb())
    {
      CHECK(i->horizontalSubsampling == 1);
      CHECK(i->verticalSubsampling == 1);
    }
    // Exactly one of the model predicates may hold.
    const int n = int(i->isRgb()) + int(i->isYuv()) + int(i->isAchromatic());
    CHECK(n == 1);
  }
}

TEST_CASE("multi-byte samples declare a byte order", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    // A format whose primary block is one byte per pixel has no order to state.
    if(i->blockPixels == 1 && i->blockBytes == 1)
      CHECK(i->byteOrder == vpf::ByteOrder::NA);
  }
}

TEST_CASE("unknown / out-of-range formats return the sentinel", "[gfx][pixfmt]")
{
  for(auto f : {V::Unknown, V(1234), V(65535), V(200)})
  {
    const auto& u = vpf::formatInfo(f);
    CHECK(std::string(u.name) == "unknown");
    CHECK_FALSE(u.valid());
    CHECK(vpf::rowBytes(f, 1920) == 0);
    CHECK(vpf::defaultStride(f, 1920) == 0);
    CHECK(vpf::bytesPerFrame(f, 1920, 1080) == 0);
  }
}

TEST_CASE("degenerate sizes yield zero, never garbage", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    CHECK(vpf::rowBytes(i->format, 0) == 0);
    CHECK(vpf::bytesPerFrame(i->format, 0, 1080) == 0);
    CHECK(vpf::bytesPerFrame(i->format, 1920, 0) == 0);
  }
}

TEST_CASE("rowBytes is the exact block count, and monotonic", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    std::size_t prev = 0;
    for(auto w : kWidths)
    {
      const auto blocks = (std::size_t(w) + i->blockPixels - 1) / i->blockPixels;
      const auto expect = blocks * i->blockBytes;
      CHECK(vpf::rowBytes(i->format, w) == expect);
      // Never narrower as the raster widens.
      CHECK(vpf::rowBytes(i->format, w) >= prev);
      prev = vpf::rowBytes(i->format, w);
    }
  }
}

TEST_CASE("alignedRowBytes pads without ever losing data", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    for(auto w : kWidths)
    {
      const auto tight = vpf::rowBytes(i->format, w);
      for(std::size_t a : {std::size_t(0), std::size_t(1), std::size_t(64),
                           std::size_t(128), std::size_t(256), std::size_t(512)})
      {
        const auto padded = vpf::alignedRowBytes(i->format, w, a);
        CHECK(padded >= tight);
        if(a > 1)
        {
          CHECK(padded % a == 0);
          // Padding never wastes a whole alignment unit.
          CHECK(padded - tight < a);
        }
        else
        {
          CHECK(padded == tight);
        }
      }
      // The convenience form must agree with the explicit one.
      CHECK(vpf::defaultStride(i->format, w)
            == vpf::alignedRowBytes(i->format, w, i->preferredStrideAlignment));
    }
  }
}

TEST_CASE("hand-computed strides: packed formats", "[gfx][pixfmt]")
{
  // 4 bytes per pixel, 256-aligned.
  CHECK(vpf::rowBytes(V::BGRA8, 1920) == 7680);
  CHECK(vpf::defaultStride(V::BGRA8, 1920) == 7680);
  CHECK(vpf::rowBytes(V::BGRA8, 1921) == 7684);
  CHECK(vpf::defaultStride(V::BGRA8, 1921) == 7936);
  // The X-padded variants are byte-identical to their alpha-bearing twins.
  CHECK(vpf::rowBytes(V::BGRX8, 1920) == vpf::rowBytes(V::BGRA8, 1920));
  CHECK(vpf::rowBytes(V::XRGB8, 1920) == vpf::rowBytes(V::ARGB8, 1920));
  // 3 bytes per pixel, 64-aligned.
  CHECK(vpf::rowBytes(V::RGB24, 1920) == 5760);
  CHECK(vpf::defaultStride(V::RGB24, 1920) == 5760);
  // 4:2:2 packed: two pixels per 4 bytes.
  CHECK(vpf::rowBytes(V::UYVY422, 1920) == 3840);
  CHECK(vpf::rowBytes(V::UYVY422, 2) == 4);
  // An odd width still occupies the whole trailing block.
  CHECK(vpf::rowBytes(V::UYVY422, 3) == 8);
  // 12-bit RGB: 8 pixels per 36 bytes.
  CHECK(vpf::rowBytes(V::R12B, 8) == 36);
  CHECK(vpf::rowBytes(V::R12B, 9) == 72);
  CHECK(vpf::rowBytes(V::R12B, 1920) == 8640);
  // 12-bit packed RGB: 2 pixels per 9 bytes.
  CHECK(vpf::rowBytes(V::RGB12P, 1920) == 8640);
  // 16-bit and float RGB.
  CHECK(vpf::rowBytes(V::RGB48, 1920) == 11520);
  CHECK(vpf::rowBytes(V::RGBA16, 1920) == 15360);
  CHECK(vpf::rowBytes(V::RGBA32F, 1920) == 30720);
  // Greyscale.
  CHECK(vpf::rowBytes(V::Mono8, 1920) == 1920);
  CHECK(vpf::rowBytes(V::Mono16, 1920) == 3840);
  CHECK(vpf::rowBytes(V::Mono16BE, 1920) == 3840);
  // Sub-byte RGB.
  CHECK(vpf::rowBytes(V::RGB332, 1920) == 1920);
  CHECK(vpf::rowBytes(V::RGB565, 1920) == 3840);
}

TEST_CASE("hand-computed strides: the v210 width rule", "[gfx][pixfmt][v210]")
{
  // SMPTE packs 48 pixels into 128 bytes; a partial group still costs 128.
  // Expressing that as the block geometry means no special case in the code.
  CHECK(vpf::rowBytes(V::V210, 48) == 128);
  CHECK(vpf::rowBytes(V::V210, 49) == 256);
  CHECK(vpf::rowBytes(V::V210, 96) == 256);
  CHECK(vpf::rowBytes(V::V210, 1920) == 5120);  // 1920 = 40 groups exactly
  CHECK(vpf::rowBytes(V::V210, 1921) == 5248);  // 41 groups
  CHECK(vpf::rowBytes(V::V210, 3840) == 10240);
  CHECK(vpf::rowBytes(V::V210, 7680) == 20480);
  CHECK(vpf::rowBytes(V::V210, 8192) == 21888); // 8192/48 = 170.67 -> 171
  // 1280 is the classic non-multiple: 1280/48 = 26.67 -> 27 groups.
  CHECK(vpf::rowBytes(V::V210, 1280) == 27 * 128);
  // Already 128-aligned, so the preferred alignment changes nothing.
  for(auto w : {48u, 49u, 1280u, 1920u, 1921u})
    CHECK(vpf::defaultStride(V::V210, w) == vpf::rowBytes(V::V210, w));
}

TEST_CASE("hand-computed frame sizes: packed", "[gfx][pixfmt]")
{
  CHECK(vpf::bytesPerFrame(V::BGRA8, 1920, 1080) == 7680u * 1080u);
  CHECK(vpf::bytesPerFrame(V::UYVY422, 1920, 1080) == 3840u * 1080u);
  CHECK(vpf::bytesPerFrame(V::V210, 1920, 1080) == 5120u * 1080u);
  CHECK(vpf::bytesPerFrame(V::Mono8, 1920, 1080) == 1920u * 1080u);
}

TEST_CASE("hand-computed frame sizes: planar and semi-planar", "[gfx][pixfmt]")
{
  // NV12: luma plane + one interleaved chroma plane at half height.
  {
    const auto y = vpf::defaultStride(V::NV12, 1920) * 1080u;
    const auto uv = vpf::defaultStride(V::NV12, 1920) * 540u;
    CHECK(vpf::bytesPerFrame(V::NV12, 1920, 1080) == y + uv);
  }
  // NV21 is NV12 with the chroma pair exchanged: identical footprint.
  CHECK(vpf::bytesPerFrame(V::NV21, 1920, 1080)
        == vpf::bytesPerFrame(V::NV12, 1920, 1080));
  // YUV420P: luma + two quarter-size planes.
  {
    const auto y = vpf::defaultStride(V::YUV420P, 1920) * 1080u;
    const auto c = vpf::defaultStride(V::YUV420P, 960) * 540u;
    CHECK(vpf::bytesPerFrame(V::YUV420P, 1920, 1080) == y + 2 * c);
  }
  // YVU420P swaps the planes only: identical footprint.
  CHECK(vpf::bytesPerFrame(V::YVU420P, 1920, 1080)
        == vpf::bytesPerFrame(V::YUV420P, 1920, 1080));
  // 10-bit planar uses 2-byte lanes, so the *tight* footprint is exactly double
  // the 8-bit one. It is only exact untight: alignment padding is not linear in
  // the sample size (at 1920, 8-bit luma pads 1920 -> 2048 while 10-bit luma is
  // already 256-aligned at 3840), so the padded sizes are merely ordered.
  const auto tightFrame = [](V f, uint32_t w, uint32_t h) -> std::size_t {
    const auto& i = vpf::formatInfo(f);
    const auto y = vpf::rowBytes(f, w) * std::size_t(h);
    if(!i.isPlanar())
      return y;
    const auto cw = w / i.horizontalSubsampling;
    const auto ch = std::size_t(h / i.verticalSubsampling);
    if(i.planeCount == 2)
      return y + vpf::rowBytes(f, cw * 2) * ch;
    return y + 2 * (vpf::rowBytes(f, cw) * ch);
  };
  CHECK(tightFrame(V::YUV420P10, 1920, 1080) == 2 * tightFrame(V::YUV420P, 1920, 1080));
  CHECK(tightFrame(V::P010, 1920, 1080) == 2 * tightFrame(V::NV12, 1920, 1080));
  CHECK(vpf::bytesPerFrame(V::YUV420P10, 1920, 1080)
        > vpf::bytesPerFrame(V::YUV420P, 1920, 1080));
  CHECK(vpf::bytesPerFrame(V::P010, 1920, 1080)
        > vpf::bytesPerFrame(V::NV12, 1920, 1080));
  // 4:2:2 planar has full-height chroma, 4:4:4 full-size chroma.
  CHECK(vpf::bytesPerFrame(V::YUV422P, 1920, 1080)
        > vpf::bytesPerFrame(V::YUV420P, 1920, 1080));
  CHECK(vpf::bytesPerFrame(V::YUV444P, 1920, 1080)
        > vpf::bytesPerFrame(V::YUV422P, 1920, 1080));
  // 12-bit shares the 16-bit lane with 10-bit, so the footprints match.
  CHECK(vpf::bytesPerFrame(V::YUV422P12, 1920, 1080)
        == vpf::bytesPerFrame(V::YUV422P10, 1920, 1080));
}

TEST_CASE("every planar format sums its planes", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    if(!i->isPlanar())
      continue;
    INFO("format " << i->name);
    const uint32_t w = 1920, h = 1080;
    const auto y = vpf::defaultStride(i->format, w) * std::size_t(h);
    const auto cw = w / i->horizontalSubsampling;
    const auto ch = std::size_t(h / i->verticalSubsampling);
    std::size_t expect = y;
    if(i->planeCount == 2)
      expect += vpf::defaultStride(i->format, cw * 2) * ch;
    else if(i->planeCount == 3)
      expect += 2 * (vpf::defaultStride(i->format, cw) * ch);
    else if(i->planeCount == 4)
      expect += 2 * (vpf::defaultStride(i->format, cw) * ch) + y;
    CHECK(vpf::bytesPerFrame(i->format, w, h) == expect);
    // A planar frame is always at least as big as its luma plane.
    CHECK(vpf::bytesPerFrame(i->format, w, h) >= y);
  }
}

TEST_CASE("AV bridge: score -> AV -> score round-trip", "[gfx][pixfmt][av]")
{
  int twins = 0;
  for(const auto* i : described())
  {
    const auto av = vpf::toAVPixelFormat(i->format);
    if(av == AV_PIX_FMT_NONE)
      continue;
    ++twins;
    INFO("format " << i->name << " -> " << av_get_pix_fmt_name(av));
    CHECK(vpf::fromAVPixelFormat(av) == i->format);
  }
  // The bridge is meant to cover most of the vocabulary; a collapse to a
  // handful would mean the switch lost its cases.
  CHECK(twins >= 40);
}

TEST_CASE("AV bridge: AV -> score -> AV round-trip", "[gfx][pixfmt][av]")
{
  for(const AVPixFmtDescriptor* d = av_pix_fmt_desc_next(nullptr); d;
      d = av_pix_fmt_desc_next(d))
  {
    const auto av = av_pix_fmt_desc_get_id(d);
    const auto f = vpf::fromAVPixelFormat(av);
    if(f == V::Unknown)
      continue;
    INFO("AV " << av_get_pix_fmt_name(av) << " -> " << vpf::formatName(f));
    CHECK(vpf::toAVPixelFormat(f) == av);
  }
}

TEST_CASE("AV bridge: unmapped inputs return the sentinel", "[gfx][pixfmt][av]")
{
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_NONE) == V::Unknown);
  // Hardware-surface and paletted formats have no place in this vocabulary.
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_VAAPI) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_CUDA) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_DRM_PRIME) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_PAL8) == V::Unknown);
  CHECK(vpf::toAVPixelFormat(V::Unknown) == AV_PIX_FMT_NONE);
  // The wire-only formats are the reason this vocabulary exists: FFmpeg models
  // them as codecs, so they must not claim a pixel-format twin.
  for(auto f : {V::V210, V::V216, V::R210, V::RGB10, V::R12B, V::R12L, V::ARGB10,
                V::DPX10, V::DPX10LE, V::RGB12P})
  {
    INFO("wire-only format " << vpf::formatName(f));
    CHECK(vpf::toAVPixelFormat(f) == AV_PIX_FMT_NONE);
  }
  // YVU420P must not alias onto yuv420p: FFmpeg expresses YV12 by swapping the
  // U and V pointers, so claiming a twin here would silently swap chroma.
  CHECK(vpf::toAVPixelFormat(V::YVU420P) == AV_PIX_FMT_NONE);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_YUV420P) == V::YUV420P);
}

TEST_CASE("descriptors agree with FFmpeg for every mapped format",
          "[gfx][pixfmt][av]")
{
  for(const auto* i : described())
  {
    const auto av = vpf::toAVPixelFormat(i->format);
    if(av == AV_PIX_FMT_NONE)
      continue;
    const AVPixFmtDescriptor* d = av_pix_fmt_desc_get(av);
    REQUIRE(d != nullptr);
    INFO("format " << i->name << " vs FFmpeg " << av_get_pix_fmt_name(av));

    CHECK(i->planeCount == av_pix_fmt_count_planes(av));
    CHECK(i->horizontalSubsampling == (1 << d->log2_chroma_w));
    CHECK(i->verticalSubsampling == (1 << d->log2_chroma_h));
    CHECK(i->isPlanar() == ((d->flags & AV_PIX_FMT_FLAG_PLANAR) != 0));
    CHECK(i->hasAlpha == ((d->flags & AV_PIX_FMT_FLAG_ALPHA) != 0));
    // FFmpeg's RGB flag means "components are R/G/B rather than luma+chroma",
    // which is true of a Bayer mosaic too. We keep Bayer as its own colour model
    // because a demosaic has to run before the samples are RGB in any usable
    // sense, so the two agree on everything except that naming.
    CHECK((i->isRgb() || i->colorModel == ColorModel::Bayer)
          == ((d->flags & AV_PIX_FMT_FLAG_RGB) != 0));
    // FFmpeg marks big-endian layouts explicitly; little-endian and
    // order-agnostic ones must not carry the flag.
    const bool avBigEndian = (d->flags & AV_PIX_FMT_FLAG_BE) != 0;
    CHECK((i->byteOrder == vpf::ByteOrder::Big) == avBigEndian);
  }
}

TEST_CASE("chroma-swapped twins are declared consistently", "[gfx][pixfmt]")
{
  for(const auto* i : described())
  {
    const auto twin = vpf::chromaSwappedTwin(i->format);
    if(twin == V::Unknown)
      continue;
    INFO("format " << i->name << " twin " << vpf::formatName(twin));
    const auto& t = vpf::formatInfo(twin);
    // A swap changes which plane holds which component, nothing else: the two
    // must agree on geometry, or one of the descriptors is wrong.
    CHECK(t.planeCount == i->planeCount);
    CHECK(t.horizontalSubsampling == i->horizontalSubsampling);
    CHECK(t.verticalSubsampling == i->verticalSubsampling);
    CHECK(t.blockPixels == i->blockPixels);
    CHECK(t.blockBytes == i->blockBytes);
    CHECK(t.colorModel == i->colorModel);
    CHECK(vpf::bytesPerFrame(twin, 1920, 1080)
          == vpf::bytesPerFrame(i->format, 1920, 1080));
    // The twin is the U-before-V layout, so it is not itself swapped.
    CHECK(vpf::chromaSwappedTwin(twin) == V::Unknown);
    // FFmpeg names the semi-planar swaps (NV21, NV42) as formats of their own,
    // but has none for the fully planar ones -- there it exchanges the U and V
    // pointers instead. Either way a fallback must exist, which is what lets
    // the camera enumeration keep offering these layouts.
    if(vpf::toAVPixelFormat(i->format) == AV_PIX_FMT_NONE)
      CHECK(vpf::toAVPixelFormat(twin) != AV_PIX_FMT_NONE);
  }
}

#if defined(__linux__)
TEST_CASE("V4L2 fourccs round-trip through the vocabulary", "[gfx][pixfmt][v4l2]")
{
  std::size_t mapped = 0;
  for(const auto* i : described())
  {
    const auto fourcc = vpf::toV4L2PixelFormat(i->format);
    if(fourcc == 0)
      continue;
    ++mapped;
    INFO("format " << i->name);
    CHECK(vpf::fromV4L2PixelFormat(fourcc) == i->format);
  }
  // The V4L2 camera formats are a large slice of the vocabulary; a collapse
  // here would mean the table lost its cases.
  CHECK(mapped >= 40);
  CHECK(vpf::fromV4L2PixelFormat(0) == V::Unknown);
  CHECK(vpf::fromV4L2PixelFormat(0xDEADBEEF) == V::Unknown);
  // Compressed fourccs are on the codec axis and must not resolve to a layout.
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_MJPEG) == V::Unknown);
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_JPEG) == V::Unknown);
}
#endif

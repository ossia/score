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
//   - cross-validation of every mapped format against av_pix_fmt_desc_get():
//     plane count, chroma subsampling, planarity, alpha and RGB-ness

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>
#include <Gfx/Graph/interop/DirectShowPixelFormat.hpp>
#include <Gfx/Graph/interop/DrmPixelFormat.hpp>
#include <Gfx/Graph/interop/GStreamerPixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatQRhi.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>
#if defined(__linux__)
#include <Gfx/Graph/interop/V4L2PixelFormat.hpp>
#include <linux/videodev2.h>
#endif

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <string_view>
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

// The values are serialized into saved documents, so every one is frozen here as
// a literal. Generating these from SCORE_VIDEO_PIXEL_FORMATS would be a
// tautology and catch nothing; written out, a renumbering breaks the build,
// which is what the table header promises.
static_assert(uint16_t(V::Unknown) == 0);
static_assert(uint16_t(V::BGRA8) == 1);
static_assert(uint16_t(V::RGBA8) == 2);
static_assert(uint16_t(V::ARGB8) == 3);
static_assert(uint16_t(V::ABGR8) == 4);
static_assert(uint16_t(V::RGB24) == 5);
static_assert(uint16_t(V::BGR24) == 6);
static_assert(uint16_t(V::BGRX8) == 7);
static_assert(uint16_t(V::RGBX8) == 8);
static_assert(uint16_t(V::XRGB8) == 9);
static_assert(uint16_t(V::XBGR8) == 19);
static_assert(uint16_t(V::R210) == 10);
static_assert(uint16_t(V::R12B) == 11);
static_assert(uint16_t(V::R12L) == 12);
static_assert(uint16_t(V::ARGB10) == 13);
static_assert(uint16_t(V::DPX10) == 14);
static_assert(uint16_t(V::DPX10LE) == 15);
static_assert(uint16_t(V::RGB12P) == 16);
static_assert(uint16_t(V::RGB48) == 17);
static_assert(uint16_t(V::RGB10) == 18);
static_assert(uint16_t(V::X2RGB10) == 120);
static_assert(uint16_t(V::X2BGR10) == 121);
static_assert(uint16_t(V::RGB332) == 90);
static_assert(uint16_t(V::RGB565) == 91);
static_assert(uint16_t(V::RGB565BE) == 92);
static_assert(uint16_t(V::RGB555) == 93);
static_assert(uint16_t(V::RGB555BE) == 94);
static_assert(uint16_t(V::ARGB1555) == 95);
static_assert(uint16_t(V::RGB444) == 96);
static_assert(uint16_t(V::ARGB4444) == 97);
static_assert(uint16_t(V::AYUV4444) == 98);
static_assert(uint16_t(V::AYUV1555) == 99);
static_assert(uint16_t(V::YUV565) == 100);
static_assert(uint16_t(V::UYVY422) == 20);
static_assert(uint16_t(V::YUYV422) == 21);
static_assert(uint16_t(V::YVYU422) == 22);
static_assert(uint16_t(V::VYUY422) == 23);
static_assert(uint16_t(V::V210) == 30);
static_assert(uint16_t(V::V216) == 31);
static_assert(uint16_t(V::Y210) == 106);
static_assert(uint16_t(V::Y216) == 107);
static_assert(uint16_t(V::NV12) == 40);
static_assert(uint16_t(V::P010) == 41);
static_assert(uint16_t(V::YUV420P) == 42);
static_assert(uint16_t(V::YUV420P10) == 43);
static_assert(uint16_t(V::NV21) == 44);
static_assert(uint16_t(V::YVU420P) == 45);
static_assert(uint16_t(V::P210) == 50);
static_assert(uint16_t(V::YUV422P) == 51);
static_assert(uint16_t(V::YUV422P10) == 52);
static_assert(uint16_t(V::NV16) == 53);
static_assert(uint16_t(V::NV61) == 54);
static_assert(uint16_t(V::YUV422P12) == 55);
static_assert(uint16_t(V::YUV422P16) == 110);
static_assert(uint16_t(V::YVU422P) == 104);
static_assert(uint16_t(V::P216) == 108);
static_assert(uint16_t(V::YUV411P) == 101);
static_assert(uint16_t(V::YUV410P) == 102);
static_assert(uint16_t(V::YVU410P) == 103);
static_assert(uint16_t(V::UYYVYY411) == 105);
static_assert(uint16_t(V::YUV444P) == 60);
static_assert(uint16_t(V::YUV444P10) == 61);
static_assert(uint16_t(V::YUV444P12) == 62);
static_assert(uint16_t(V::NV24) == 63);
static_assert(uint16_t(V::NV42) == 64);
static_assert(uint16_t(V::VUYA) == 65);
static_assert(uint16_t(V::VUYX) == 66);
static_assert(uint16_t(V::AYUV) == 67);
static_assert(uint16_t(V::XYUV) == 68);
static_assert(uint16_t(V::YUVA) == 69);
static_assert(uint16_t(V::YUVX) == 73);
static_assert(uint16_t(V::YUVA444P) == 111);
static_assert(uint16_t(V::P416) == 109);
static_assert(uint16_t(V::XV30) == 112);
static_assert(uint16_t(V::AYUV64) == 113);
static_assert(uint16_t(V::RGBA16) == 70);
static_assert(uint16_t(V::RGBA16F) == 71);
static_assert(uint16_t(V::RGBA32F) == 72);
static_assert(uint16_t(V::Mono8) == 80);
static_assert(uint16_t(V::Mono10) == 81);
static_assert(uint16_t(V::Mono12) == 82);
static_assert(uint16_t(V::Mono16) == 83);
static_assert(uint16_t(V::BayerRG8) == 84);
static_assert(uint16_t(V::BayerRG12) == 85);
static_assert(uint16_t(V::BayerBGGR8) == 114);
static_assert(uint16_t(V::BayerGBRG8) == 115);
static_assert(uint16_t(V::BayerGRBG8) == 116);
static_assert(uint16_t(V::BayerRGGB8) == 117);
static_assert(uint16_t(V::BayerBGGR16) == 118);
static_assert(uint16_t(V::BayerRGGB16) == 119);
static_assert(uint16_t(V::Mono16BE) == 86);

TEST_CASE("the vocabulary is non-trivial and self-consistent", "[gfx][pixfmt]")
{
  const auto all = described();
  REQUIRE(all.size() == vpf::formatCount());
  // Guards against the table being accidentally emptied or halved.
  CHECK(all.size() == 90);

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
  // 36 bits per pixel is exactly 2 pixels per 9 bytes, and all three 12-bit
  // layouts share that granularity. (width * 36) / 8, the vendor row formula,
  // agrees at every even width.
  for(auto f : {V::R12B, V::R12L, V::RGB12P})
  {
    INFO("format " << vpf::formatName(f));
    CHECK(vpf::rowBytes(f, 2) == 9);
    CHECK(vpf::rowBytes(f, 8) == 36);
    CHECK(vpf::rowBytes(f, 9) == 45);
    CHECK(vpf::rowBytes(f, 1920) == 8640);
    CHECK(vpf::rowBytes(f, 1922) == (1922u * 36u) / 8u);
  }
  // r210 pads to a 64-pixel/256-byte row, and that is mandatory rather than a
  // preference: a tight row is not a legal r210 frame.
  CHECK(vpf::rowBytes(V::R210, 64) == 256);
  CHECK(vpf::rowBytes(V::R210, 65) == 512);
  CHECK(vpf::rowBytes(V::R210, 1920) == 7680);
  CHECK(vpf::rowBytes(V::R210, 1921) == 7936);
  CHECK(vpf::rowBytes(V::R210, 1921) == ((1921u + 63u) / 64u) * 256u);
  // so the preferred alignment adds nothing on top
  for(auto w : {64u, 65u, 1920u, 1921u})
    CHECK(vpf::defaultStride(V::R210, w) == vpf::rowBytes(V::R210, w));
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
  // Exact, not a floor: a floor lets a batch of mappings be deleted silently.
  CHECK(twins == 60);
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
  // Exact, for the same reason as the AV count.
  CHECK(mapped == 54);
  CHECK(vpf::fromV4L2PixelFormat(0) == V::Unknown);
  CHECK(vpf::fromV4L2PixelFormat(0xDEADBEEF) == V::Unknown);
  // Compressed fourccs are on the codec axis and must not resolve to a layout.
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_MJPEG) == V::Unknown);
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_JPEG) == V::Unknown);
}
#endif

TEST_CASE("bytesPerFrame rounds chroma up at odd sizes", "[gfx][pixfmt]")
{
  // Truncating the chroma dimensions loses a whole row or column, and the
  // shortfall is invisible at 1920x1080 because both dimensions divide evenly.
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    for(uint32_t w : {1u, 3u, 7u, 719u, 1921u})
    {
      for(uint32_t h : {1u, 3u, 1081u})
      {
        const auto cw
            = (w + i->horizontalSubsampling - 1) / i->horizontalSubsampling;
        const auto ch = std::size_t((h + i->verticalSubsampling - 1)
                                    / i->verticalSubsampling);
        const auto y = vpf::defaultStride(i->format, w) * std::size_t(h);
        std::size_t expect = y;
        if(i->planeCount == 2)
          expect += vpf::defaultStride(i->format, cw * 2) * ch;
        else if(i->planeCount == 3)
          expect += 2 * (vpf::defaultStride(i->format, cw) * ch);
        else if(i->planeCount == 4)
          expect += 2 * (vpf::defaultStride(i->format, cw) * ch) + y;
        CHECK(vpf::bytesPerFrame(i->format, w, h) == expect);
      }
    }
  }
  // Going from an odd to the next even row count grows luma by one row and
  // leaves chroma alone, since the odd count already paid for its chroma row.
  // Under the old truncating arithmetic the difference was two rows.
  for(auto f : {V::NV12, V::YUV420P, V::P010})
  {
    INFO("format " << vpf::formatName(f));
    CHECK(vpf::bytesPerFrame(f, 1920, 1082) - vpf::bytesPerFrame(f, 1920, 1081)
          == vpf::defaultStride(f, 1920));
  }
  // and strictly more than the truncating answer did
  CHECK(vpf::bytesPerFrame(V::NV12, 1920, 1081)
        > vpf::bytesPerFrame(V::NV12, 1920, 1080));
}

// Every AVPixelFormat mapping, pinned to the FFmpeg format NAME rather than to
// the enumerator. The round-trip tests only compose toAV and fromAV, so any
// self-consistent permutation between two formats of identical geometry -- BGRA8
// with RGBA8, NV12 with NV21 -- satisfies them, and satisfies the descriptor
// cross-check too, since those pairs agree on plane count, subsampling, alpha
// and endianness. Component-order confusion is the bug class this vocabulary
// exists to prevent, so it has to be nailed down by name.
TEST_CASE("every AV mapping is pinned to a named FFmpeg format", "[gfx][pixfmt][av]")
{
  struct Pin { V f; const char* av; };
  static constexpr Pin kPinned[] = {
    {V::BGRA8, "bgra"},
    {V::RGBA8, "rgba"},
    {V::ARGB8, "argb"},
    {V::ABGR8, "abgr"},
    {V::RGB24, "rgb24"},
    {V::BGR24, "bgr24"},
    {V::BGRX8, "bgr0"},
    {V::RGBX8, "rgb0"},
    {V::XRGB8, "0rgb"},
    {V::XBGR8, "0bgr"},
    {V::RGB48, "rgb48le"},
    {V::X2RGB10, "x2rgb10le"},
    {V::X2BGR10, "x2bgr10le"},
    {V::RGB565, "rgb565le"},
    {V::RGB565BE, "rgb565be"},
    {V::RGB555, "rgb555le"},
    {V::RGB555BE, "rgb555be"},
    {V::RGB444, "rgb444le"},
    {V::UYVY422, "uyvy422"},
    {V::YUYV422, "yuyv422"},
    {V::YVYU422, "yvyu422"},
    {V::Y210, "y210le"},
    {V::NV12, "nv12"},
    {V::P010, "p010le"},
    {V::YUV420P, "yuv420p"},
    {V::YUV420P10, "yuv420p10le"},
    {V::NV21, "nv21"},
    {V::P210, "p210le"},
    {V::YUV422P, "yuv422p"},
    {V::YUV422P10, "yuv422p10le"},
    {V::NV16, "nv16"},
    {V::YUV422P12, "yuv422p12le"},
    {V::YUV422P16, "yuv422p16le"},
    {V::P216, "p216le"},
    {V::YUV411P, "yuv411p"},
    {V::YUV410P, "yuv410p"},
    {V::UYYVYY411, "uyyvyy411"},
    {V::YUV444P, "yuv444p"},
    {V::YUV444P10, "yuv444p10le"},
    {V::YUV444P12, "yuv444p12le"},
    {V::NV24, "nv24"},
    {V::NV42, "nv42"},
    {V::VUYA, "vuya"},
    {V::VUYX, "vuyx"},
    {V::YUVA444P, "yuva444p"},
    {V::P416, "p416le"},
    {V::XV30, "xv30le"},
    {V::AYUV64, "ayuv64le"},
    {V::RGBA16, "rgba64le"},
    {V::Mono8, "gray"},
    {V::Mono10, "gray10le"},
    {V::Mono12, "gray12le"},
    {V::Mono16, "gray16le"},
    {V::BayerBGGR8, "bayer_bggr8"},
    {V::BayerGBRG8, "bayer_gbrg8"},
    {V::BayerGRBG8, "bayer_grbg8"},
    {V::BayerRGGB8, "bayer_rggb8"},
    {V::BayerBGGR16, "bayer_bggr16le"},
    {V::BayerRGGB16, "bayer_rggb16le"},
    {V::Mono16BE, "gray16be"},
  };
  std::set<V> pinned;
  for(auto [f, name] : kPinned)
  {
    INFO(vpf::formatName(f) << " must map to " << name);
    const auto want = av_get_pix_fmt(name);
    REQUIRE(want != AV_PIX_FMT_NONE);
    CHECK(vpf::toAVPixelFormat(f) == want);
    CHECK(vpf::fromAVPixelFormat(want) == f);
    pinned.insert(f);
  }
  // No mapping may exist that this list does not pin, so adding one without
  // pinning it fails here rather than going unverified.
  for(const auto* i : described())
  {
    if(vpf::toAVPixelFormat(i->format) != AV_PIX_FMT_NONE)
    {
      INFO(i->name << " has an AV mapping but is not pinned");
      CHECK(pinned.count(i->format) == 1);
    }
  }
  CHECK(pinned.size() == 60);
}

#if defined(__linux__)
// Every V4L2 fourcc pinned to the kernel constant. The round-trip sweep only
// composes to/from, so swapping two same-geometry formats in BOTH directions --
// NV12 with NV21 -- satisfies it while every camera silently delivers exchanged
// chroma.
TEST_CASE("every V4L2 mapping is pinned to a kernel constant", "[gfx][pixfmt][v4l2]")
{
  struct Pin { V f; uint32_t fourcc; };
  static const Pin kFourcc[] = {
      {V::UYVY422, V4L2_PIX_FMT_UYVY},
      {V::YUYV422, V4L2_PIX_FMT_YUYV},
      {V::YVYU422, V4L2_PIX_FMT_YVYU},
      {V::VYUY422, V4L2_PIX_FMT_VYUY},
      {V::NV12, V4L2_PIX_FMT_NV12},
      {V::NV21, V4L2_PIX_FMT_NV21},
      {V::NV16, V4L2_PIX_FMT_NV16},
      {V::NV61, V4L2_PIX_FMT_NV61},
      {V::NV24, V4L2_PIX_FMT_NV24},
      {V::NV42, V4L2_PIX_FMT_NV42},
      {V::YUV420P, V4L2_PIX_FMT_YUV420},
      {V::YVU420P, V4L2_PIX_FMT_YVU420},
      {V::YUV422P, V4L2_PIX_FMT_YUV422P},
      {V::YUV411P, V4L2_PIX_FMT_YUV411P},
      {V::YUV410P, V4L2_PIX_FMT_YUV410},
      {V::YVU410P, V4L2_PIX_FMT_YVU410},
      {V::VUYA, V4L2_PIX_FMT_VUYA32},
      {V::VUYX, V4L2_PIX_FMT_VUYX32},
      {V::AYUV, V4L2_PIX_FMT_AYUV32},
      {V::XYUV, V4L2_PIX_FMT_XYUV32},
      {V::YUVA, V4L2_PIX_FMT_YUVA32},
      {V::YUVX, V4L2_PIX_FMT_YUVX32},
      {V::AYUV4444, V4L2_PIX_FMT_YUV444},
      {V::AYUV1555, V4L2_PIX_FMT_YUV555},
      {V::YUV565, V4L2_PIX_FMT_YUV565},
      {V::ARGB8, V4L2_PIX_FMT_ARGB32},
      {V::XRGB8, V4L2_PIX_FMT_XRGB32},
      {V::BGRA8, V4L2_PIX_FMT_ABGR32},
      {V::BGRX8, V4L2_PIX_FMT_XBGR32},
#ifdef V4L2_PIX_FMT_RGBA32
      {V::RGBA8, V4L2_PIX_FMT_RGBA32},
#endif
#ifdef V4L2_PIX_FMT_RGBA32
      {V::RGBX8, V4L2_PIX_FMT_RGBX32},
#endif
#ifdef V4L2_PIX_FMT_RGBA32
      {V::ABGR8, V4L2_PIX_FMT_BGRA32},
#endif
#ifdef V4L2_PIX_FMT_RGBA32
      {V::XBGR8, V4L2_PIX_FMT_BGRX32},
#endif
      {V::RGB24, V4L2_PIX_FMT_RGB24},
      {V::BGR24, V4L2_PIX_FMT_BGR24},
      {V::RGB332, V4L2_PIX_FMT_RGB332},
      {V::RGB565, V4L2_PIX_FMT_RGB565},
      {V::RGB565BE, V4L2_PIX_FMT_RGB565X},
      {V::RGB555, V4L2_PIX_FMT_RGB555},
      {V::RGB555BE, V4L2_PIX_FMT_RGB555X},
      {V::ARGB1555, V4L2_PIX_FMT_ARGB555},
      {V::ARGB4444, V4L2_PIX_FMT_ARGB444},
      {V::RGB444, V4L2_PIX_FMT_RGB444},
      {V::Mono8, V4L2_PIX_FMT_GREY},
      {V::Mono10, V4L2_PIX_FMT_Y10},
      {V::Mono12, V4L2_PIX_FMT_Y12},
      {V::Mono16, V4L2_PIX_FMT_Y16},
      {V::Mono16BE, V4L2_PIX_FMT_Y16_BE},
      {V::BayerBGGR8, V4L2_PIX_FMT_SBGGR8},
      {V::BayerGBRG8, V4L2_PIX_FMT_SGBRG8},
      {V::BayerGRBG8, V4L2_PIX_FMT_SGRBG8},
      {V::BayerRGGB8, V4L2_PIX_FMT_SRGGB8},
#ifdef V4L2_PIX_FMT_SBGGR16
      {V::BayerBGGR16, V4L2_PIX_FMT_SBGGR16},
#endif
#ifdef V4L2_PIX_FMT_SRGGB16
      {V::BayerRGGB16, V4L2_PIX_FMT_SRGGB16},
#endif
  };
  std::set<V> pinned;
  for(auto [f, fourcc] : kFourcc)
  {
    INFO(vpf::formatName(f));
    CHECK(vpf::toV4L2PixelFormat(f) == fourcc);
    CHECK(vpf::fromV4L2PixelFormat(fourcc) == f);
    pinned.insert(f);
  }
  for(const auto* i : described())
  {
    if(vpf::toV4L2PixelFormat(i->format) != 0)
    {
      INFO(i->name << " has a fourcc but is not pinned");
      CHECK(pinned.count(i->format) == 1);
    }
  }
  // The one-way aliases can never be reached by a score->fourcc->score sweep, so
  // they need naming. The deprecated RGB32/BGR32 pair is where the two old
  // tables disagreed.
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_RGB32) == V::XRGB8);
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_BGR32) == V::BGRX8);
#ifdef V4L2_PIX_FMT_Z16
  CHECK(vpf::fromV4L2PixelFormat(V4L2_PIX_FMT_Z16) == V::Mono16);
#endif
}
#endif

// chromaSwappedTwin needs positive coverage: the sweep over described formats
// skips anything returning Unknown, so deleting every case would pass vacuously.
TEST_CASE("the chroma-swapped layouts each declare their twin", "[gfx][pixfmt]")
{
  struct T { V swapped; V twin; };
  static constexpr T kTwins[] = {
      {V::YVU420P, V::YUV420P}, {V::YVU422P, V::YUV422P},
      {V::YVU410P, V::YUV410P}, {V::NV21, V::NV12},
      {V::NV61, V::NV16},       {V::NV42, V::NV24}};
  for(auto [a, b] : kTwins)
  {
    INFO(vpf::formatName(a));
    CHECK(vpf::chromaSwappedTwin(a) == b);
  }
  int found = 0;
  for(const auto* i : described())
    if(vpf::chromaSwappedTwin(i->format) != V::Unknown)
      ++found;
  CHECK(found == int(std::size(kTwins)));
  // Structural, so a newly added V-first layout fails until it declares a twin.
  for(const auto* i : described())
  {
    const std::string_view n{i->name};
    if(n.substr(0, 3) == "YVU" || n == "NV21" || n == "NV61" || n == "NV42")
    {
      INFO(i->name << " is a V-first layout with no twin declared");
      CHECK(vpf::chromaSwappedTwin(i->format) != V::Unknown);
    }
  }
}

// Cross-check rowBytes against FFmpeg's own linesize. This catches a wrong
// blockBytes mechanically and with no second list: the rowBytes sweep derives its
// expectation from blockBytes, so it agrees with whatever value is put there.
TEST_CASE("rowBytes agrees with the FFmpeg linesize", "[gfx][pixfmt][av]")
{
  for(const auto* i : described())
  {
    const auto av = vpf::toAVPixelFormat(i->format);
    if(av == AV_PIX_FMT_NONE)
      continue;
    for(uint32_t w : {16u, 48u, 64u, 720u, 1920u, 3840u})
    {
      const int ls = av_image_get_linesize(av, int(w), 0);
      if(ls <= 0)
        continue;
      INFO(i->name << " at width " << w);
      CHECK(vpf::rowBytes(i->format, w) == std::size_t(ls));
    }
  }
}

// The 30 formats FFmpeg cannot vouch for, frozen field by field.
//
// Deriving everything from one table is what makes the AV-mapped 58 verifiable
// against FFmpeg with no maintenance, and it is also what leaves these 30
// unverifiable: a mutation to any field of a wire-only format changes both the
// code and every expectation derived from it. So each gets a golden row, audited
// against the vendor specifications -- DeckLink and AJA SDK row-size formulas,
// Apple TN2162 for v210/v216, GenICam PFNC for the Bayer naming.
TEST_CASE("wire-only descriptors are frozen", "[gfx][pixfmt]")
{
  struct Golden
  {
    V f;
    ColorModel model;
    int planes, hsub, vsub, blockPixels, blockBytes;
    bool alpha;
    vpf::ByteOrder order;
    int align;
  };
  static constexpr Golden kGolden[] = {
      {V::R210, ColorModel::RGB, 1, 1, 1, 64, 256, false, vpf::ByteOrder::Big, 256},
      {V::R12B, ColorModel::RGB, 1, 1, 1, 2, 9, false, vpf::ByteOrder::Big, 256},
      {V::R12L, ColorModel::RGB, 1, 1, 1, 2, 9, false, vpf::ByteOrder::Little, 256},
      {V::ARGB10, ColorModel::RGB, 1, 1, 1, 1, 5, true, vpf::ByteOrder::Little, 256},
      {V::DPX10, ColorModel::RGB, 1, 1, 1, 1, 4, false, vpf::ByteOrder::Big, 256},
      {V::DPX10LE, ColorModel::RGB, 1, 1, 1, 1, 4, false, vpf::ByteOrder::Little, 256},
      {V::RGB12P, ColorModel::RGB, 1, 1, 1, 2, 9, false, vpf::ByteOrder::Little, 256},
      {V::RGB10, ColorModel::RGB, 1, 1, 1, 1, 4, false, vpf::ByteOrder::Little, 256},
      {V::RGB332, ColorModel::RGB, 1, 1, 1, 1, 1, false, vpf::ByteOrder::NA, 64},
      {V::ARGB1555, ColorModel::RGB, 1, 1, 1, 1, 2, true, vpf::ByteOrder::Little, 64},
      {V::ARGB4444, ColorModel::RGB, 1, 1, 1, 1, 2, true, vpf::ByteOrder::Little, 64},
      {V::AYUV4444, ColorModel::YUV, 1, 1, 1, 1, 2, true, vpf::ByteOrder::Little, 64},
      {V::AYUV1555, ColorModel::YUV, 1, 1, 1, 1, 2, true, vpf::ByteOrder::Little, 64},
      {V::YUV565, ColorModel::YUV, 1, 1, 1, 1, 2, false, vpf::ByteOrder::Little, 64},
      {V::VYUY422, ColorModel::YUV, 1, 2, 1, 2, 4, false, vpf::ByteOrder::NA, 256},
      {V::V210, ColorModel::YUV, 1, 2, 1, 48, 128, false, vpf::ByteOrder::Little, 128},
      {V::V216, ColorModel::YUV, 1, 2, 1, 2, 8, false, vpf::ByteOrder::Little, 256},
      {V::Y216, ColorModel::YUV, 1, 2, 1, 2, 8, false, vpf::ByteOrder::Little, 256},
      {V::YVU420P, ColorModel::YUV, 3, 2, 2, 1, 1, false, vpf::ByteOrder::NA, 256},
      {V::NV61, ColorModel::YUV, 2, 2, 1, 1, 1, false, vpf::ByteOrder::NA, 256},
      {V::YVU422P, ColorModel::YUV, 3, 2, 1, 1, 1, false, vpf::ByteOrder::NA, 256},
      {V::YVU410P, ColorModel::YUV, 3, 4, 4, 1, 1, false, vpf::ByteOrder::NA, 256},
      {V::AYUV, ColorModel::YUV, 1, 1, 1, 1, 4, true, vpf::ByteOrder::NA, 256},
      {V::XYUV, ColorModel::YUV, 1, 1, 1, 1, 4, false, vpf::ByteOrder::NA, 256},
      {V::YUVA, ColorModel::YUV, 1, 1, 1, 1, 4, true, vpf::ByteOrder::NA, 256},
      {V::YUVX, ColorModel::YUV, 1, 1, 1, 1, 4, false, vpf::ByteOrder::NA, 256},
      {V::RGBA16F, ColorModel::RGB, 1, 1, 1, 1, 8, true, vpf::ByteOrder::Little, 256},
      {V::RGBA32F, ColorModel::RGB, 1, 1, 1, 1, 16, true, vpf::ByteOrder::Little, 256},
      {V::BayerRG8, ColorModel::Bayer, 1, 1, 1, 1, 1, false, vpf::ByteOrder::NA, 64},
      {V::BayerRG12, ColorModel::Bayer, 1, 1, 1, 1, 2, false, vpf::ByteOrder::Little, 64},
  };
  std::set<V> frozen;
  for(const auto& g : kGolden)
  {
    const auto& i = vpf::formatInfo(g.f);
    INFO("format " << i.name);
    CHECK(i.colorModel == g.model);
    CHECK(i.planeCount == g.planes);
    CHECK(i.horizontalSubsampling == g.hsub);
    CHECK(i.verticalSubsampling == g.vsub);
    CHECK(i.blockPixels == g.blockPixels);
    CHECK(i.blockBytes == g.blockBytes);
    CHECK(i.hasAlpha == g.alpha);
    CHECK(i.byteOrder == g.order);
    CHECK(i.preferredStrideAlignment == g.align);
    frozen.insert(g.f);
  }
  // Every format without an AV twin must be frozen here, so adding a wire-only
  // format without a golden row fails rather than going unverified.
  for(const auto* i : described())
  {
    if(vpf::toAVPixelFormat(i->format) == AV_PIX_FMT_NONE)
    {
      INFO(i->name << " has no AV twin and no golden row");
      CHECK(frozen.count(i->format) == 1);
    }
  }
  CHECK(frozen.size() == 30);
  // and together the two sets are the whole vocabulary
  CHECK(frozen.size() + 60u == vpf::formatCount());
}

// The byteOrder rule stated in the header has two halves, and asserting only the
// first lets a multi-byte format declare NA.
//
// Whether an order is required is derivable neither from this descriptor nor
// from component depth: RGB565 has 5- and 6-bit components packed into a 16-bit
// word, so its order matters, while UYVY422 has four whole-byte components and
// no order to state. FFmpeg's naming convention encodes exactly that
// distinction -- an le/be pair only where the order is part of the identity --
// so it is the authority for the mapped formats. The wire-only ones are frozen
// by the golden table above.
TEST_CASE("byte order is declared exactly when FFmpeg spells one",
          "[gfx][pixfmt][av]")
{
  for(const auto* i : described())
  {
    const auto av = vpf::toAVPixelFormat(i->format);
    if(av == AV_PIX_FMT_NONE)
      continue;
    const std::string_view name{av_get_pix_fmt_name(av)};
    INFO("format " << i->name << " vs " << name);
    if(name.size() > 2 && name.substr(name.size() - 2) == "le")
      CHECK(i->byteOrder == vpf::ByteOrder::Little);
    else if(name.size() > 2 && name.substr(name.size() - 2) == "be")
      CHECK(i->byteOrder == vpf::ByteOrder::Big);
    else
      CHECK(i->byteOrder == vpf::ByteOrder::NA);
  }
}

// The DirectShow fourcc table, pinned by literal fourcc. This is Windows-only
// data, but keeping it in fourcc terms makes it testable on any host, which is
// the point: the mapping it replaces lived in Windows-only code and had gone
// unexercised long enough to accumulate a chroma swap and a "not sure".
TEST_CASE("DirectShow fourccs resolve to the right layout", "[gfx][pixfmt][dshow]")
{
  using vpf::directShowFourcc;
  const auto f = [](const char* s) {
    return directShowFourcc(s[0], s[1], s[2], s[3]);
  };
  struct Pin { const char* fourcc; V expect; };
  static const Pin kPins[] = {
      // packed 4:2:2
      {"YUY2", V::YUYV422}, {"YUYV", V::YUYV422}, {"UYVY", V::UYVY422},
      {"Y422", V::UYVY422}, {"YVYU", V::YVYU422},
      {"Y210", V::Y210},    {"Y216", V::Y216},    {"V216", V::V216},
      // planar; the V-before-U spellings must not collapse onto the U-first ones
      {"I420", V::YUV420P}, {"IYUV", V::YUV420P}, {"YV12", V::YVU420P},
      {"YV16", V::YVU422P}, {"YVU9", V::YVU410P},
      // semi-planar
      {"NV12", V::NV12}, {"NV21", V::NV21}, {"NV16", V::NV16},
      {"P208", V::NV16}, {"NV24", V::NV24}, {"P408", V::NV24},
      {"NV42", V::NV42}, {"P010", V::P010}, {"P210", V::P210},
      {"P216", V::P216},
      // packed 4:4:4 and 4:1:1
      {"AYUV", V::VUYA}, {"Y410", V::XV30}, {"Y416", V::AYUV64},
      {"Y41P", V::UYYVYY411},
      // single channel
      {"GREY", V::Mono8}, {"Y800", V::Mono8}, {"Y160", V::Mono16},
  };
  for(const auto& p : kPins)
  {
    INFO("fourcc " << p.fourcc);
    CHECK(vpf::fromDirectShowFourcc(f(p.fourcc)) == p.expect);
  }
  // YV12 and I420 are the same geometry and differ only in plane order, so the
  // swap must be visible in the vocabulary rather than lost.
  CHECK(vpf::chromaSwappedTwin(vpf::fromDirectShowFourcc(f("YV12")))
        == vpf::fromDirectShowFourcc(f("I420")));
  CHECK(vpf::chromaSwappedTwin(vpf::fromDirectShowFourcc(f("YV16")))
        == V::YUV422P);
  CHECK(vpf::chromaSwappedTwin(vpf::fromDirectShowFourcc(f("YVU9")))
        == V::YUV410P);
  // Compressed subtypes are on the codec axis.
  for(const char* c : {"MJPG", "TVMJ", "WAKE", "Plum", "H264"})
  {
    INFO("compressed " << c);
    CHECK(vpf::isDirectShowCompressedFourcc(f(c)));
    CHECK(vpf::fromDirectShowFourcc(f(c)) == V::Unknown);
  }
  CHECK_FALSE(vpf::isDirectShowCompressedFourcc(f("NV12")));
  CHECK(vpf::fromDirectShowFourcc(0) == V::Unknown);
}

// DRM fourccs, pinned by literal characters. A DRM name reads in machine-word
// order, so the memory byte order is its reverse -- DRM_ARGB8888 is B,G,R,A in
// memory. Getting that inversion wrong is the most common red/blue swap on the
// dma-buf path, so the direction is asserted explicitly rather than left to a
// round-trip that would accept either reading.
TEST_CASE("DRM fourccs resolve with the word-order inversion", "[gfx][pixfmt][drm]")
{
  using vpf::drmPixelFourcc;
  const auto f = [](const char* s) { return drmPixelFourcc(s[0], s[1], s[2], s[3]); };
  struct Pin { const char* fourcc; V expect; };
  static const Pin kPins[] = {
      // the inversion: the name says ARGB, memory holds B,G,R,A
      {"AR24", V::BGRA8},   {"AB24", V::RGBA8},
      {"XR24", V::BGRX8},   {"XB24", V::RGBX8},
      {"RA24", V::ABGR8},   {"BA24", V::ARGB8},
      {"RG24", V::BGR24},   {"BG24", V::RGB24},
      {"RG16", V::RGB565},
      {"AR30", V::X2RGB10}, {"AB30", V::X2BGR10},
      {"AB4H", V::RGBA16F},
      // semi-planar and planar
      {"NV12", V::NV12}, {"NV21", V::NV21}, {"NV16", V::NV16},
      {"NV61", V::NV61}, {"NV24", V::NV24}, {"NV42", V::NV42},
      {"P010", V::P010}, {"P210", V::P210}, {"P410", V::P416},
      {"YU12", V::YUV420P}, {"YV12", V::YVU420P},
      {"YU16", V::YUV422P}, {"YV16", V::YVU422P},
      {"YU24", V::YUV444P},
      // packed 4:2:2
      {"YUYV", V::YUYV422}, {"YVYU", V::YVYU422},
      {"UYVY", V::UYVY422}, {"VYUY", V::VYUY422},
      // single channel
      {"R8  ", V::Mono8}, {"R16 ", V::Mono16},
  };
  std::set<V> pinned;
  for(const auto& p : kPins)
  {
    INFO("DRM fourcc " << p.fourcc);
    CHECK(vpf::fromDrmFourcc(f(p.fourcc)) == p.expect);
    CHECK(vpf::toDrmFourcc(p.expect) == f(p.fourcc));
    pinned.insert(p.expect);
  }
  for(const auto* i : described())
  {
    if(vpf::toDrmFourcc(i->format) != 0)
    {
      INFO(i->name << " has a DRM fourcc but is not pinned");
      CHECK(pinned.count(i->format) == 1);
    }
  }
  // The V-first DRM layouts must keep their swap visible, exactly as elsewhere.
  CHECK(vpf::chromaSwappedTwin(vpf::fromDrmFourcc(f("YV12"))) == V::YUV420P);
  CHECK(vpf::chromaSwappedTwin(vpf::fromDrmFourcc(f("YV16"))) == V::YUV422P);
  CHECK(vpf::fromDrmFourcc(0) == V::Unknown);
  CHECK(vpf::toDrmFourcc(V::Unknown) == 0);
}

// The GPU-texture axis. Not a bijection with the buffer axis, and the test says
// so rather than pretending: planar layouts answer per plane, packed YUV samples
// as RGBA8 because QRhi has no YUV format, and the wire-only layouts have no
// texture at all.
TEST_CASE("plane texture formats and sizes", "[gfx][pixfmt][qrhi]")
{
  // Packed RGB keeps its own format and full width.
  CHECK(vpf::planeTextureFormat(V::BGRA8, 0) == QRhiTexture::BGRA8);
  CHECK(vpf::planeTextureWidth(V::BGRA8, 0, 1920) == 1920);
  CHECK(vpf::planeTextureHeight(V::BGRA8, 0, 1080) == 1080);

  // Packed 4:2:2 samples as RGBA8 at half the texel width: two pixels per texel.
  CHECK(vpf::planeTextureFormat(V::UYVY422, 0) == QRhiTexture::RGBA8);
  CHECK(vpf::planeTextureWidth(V::UYVY422, 0, 1920) == 960);
  CHECK(vpf::planeTextureHeight(V::UYVY422, 0, 1080) == 1080);

  // Semi-planar: R8 luma plus an RG8 chroma plane at half size.
  CHECK(vpf::planeTextureFormat(V::NV12, 0) == QRhiTexture::R8);
  CHECK(vpf::planeTextureFormat(V::NV12, 1) == QRhiTexture::RG8);
  CHECK(vpf::planeTextureWidth(V::NV12, 1, 1920) == 960);
  CHECK(vpf::planeTextureHeight(V::NV12, 1, 1080) == 540);
  CHECK(vpf::planeTextureFormat(V::NV12, 2) == QRhiTexture::UnknownFormat);

  // 10-bit semi-planar uses 16-bit lanes.
  CHECK(vpf::planeTextureFormat(V::P010, 0) == QRhiTexture::R16);
  CHECK(vpf::planeTextureFormat(V::P010, 1) == QRhiTexture::RG16);

  // Fully planar: three single-component planes.
  CHECK(vpf::planeTextureFormat(V::YUV420P, 0) == QRhiTexture::R8);
  CHECK(vpf::planeTextureFormat(V::YUV420P, 1) == QRhiTexture::R8);
  CHECK(vpf::planeTextureFormat(V::YUV420P, 2) == QRhiTexture::R8);
  CHECK(vpf::planeTextureFormat(V::YUV422P10, 1) == QRhiTexture::R16);

  // Odd sizes round the chroma plane up, as the buffer arithmetic does.
  CHECK(vpf::planeTextureWidth(V::YUV420P, 1, 1921) == 961);
  CHECK(vpf::planeTextureHeight(V::YUV420P, 1, 1081) == 541);

  // The wire-only layouts must be decoded before they are a texture.
  for(auto f : {V::V210, V::R210, V::R12B, V::RGB12P, V::DPX10})
  {
    INFO("wire-only " << vpf::formatName(f));
    CHECK(vpf::planeTextureFormat(f, 0) == QRhiTexture::UnknownFormat);
  }

  // Degenerate inputs
  CHECK(vpf::planeTextureFormat(V::Unknown, 0) == QRhiTexture::UnknownFormat);
  CHECK(vpf::planeTextureWidth(V::BGRA8, 0, 0) == 0);
  CHECK(vpf::planeTextureWidth(V::BGRA8, -1, 1920) == 0);

  // Every format either offers a plane-0 texture or is a decode-first layout;
  // none may answer a format for a plane it does not have.
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    CHECK(vpf::planeTextureFormat(i->format, i->planeCount)
          == QRhiTexture::UnknownFormat);
    for(int p = 0; p < i->planeCount; ++p)
    {
      if(vpf::planeTextureFormat(i->format, p) != QRhiTexture::UnknownFormat)
      {
        CHECK(vpf::planeTextureWidth(i->format, p, 1920) > 0);
        CHECK(vpf::planeTextureHeight(i->format, p, 1080) > 0);
      }
    }
  }
}

// The texture-to-buffer direction is for transports that hand over a texture:
// Spout a D3D11 texture, Syphon an IOSurface, dma-buf an EGLImage.
TEST_CASE("texture formats map back only where unambiguous", "[gfx][pixfmt][qrhi]")
{
  CHECK(vpf::fromTextureFormat(QRhiTexture::BGRA8) == V::BGRA8);
  CHECK(vpf::fromTextureFormat(QRhiTexture::RGBA8) == V::RGBA8);
  CHECK(vpf::fromTextureFormat(QRhiTexture::RGBA16F) == V::RGBA16F);
  CHECK(vpf::fromTextureFormat(QRhiTexture::RGBA32F) == V::RGBA32F);
  CHECK(vpf::fromTextureFormat(QRhiTexture::R8) == V::Mono8);
  CHECK(vpf::fromTextureFormat(QRhiTexture::R16) == V::Mono16);
  // Depth and compressed textures are not video buffers.
  for(auto t : {QRhiTexture::D16, QRhiTexture::D32F, QRhiTexture::BC1,
                QRhiTexture::UnknownFormat})
  {
    CHECK(vpf::fromTextureFormat(t) == V::Unknown);
  }
  // And the round-trip holds for the ones that do map back.
  for(auto f : {V::BGRA8, V::RGBA8, V::RGBA16F, V::RGBA32F, V::Mono8, V::Mono16})
  {
    INFO(vpf::formatName(f));
    CHECK(vpf::fromTextureFormat(vpf::planeTextureFormat(f, 0)) == f);
  }
}

// GStreamer names, for the transports that hand over a buffer rather than a
// stream. Composing the existing gst->AV map with the libav bridge would lose the
// V-before-U layouts, so this asserts the direct table gets them right -- that is
// the whole reason it exists.
TEST_CASE("GStreamer names resolve to the right layout", "[gfx][pixfmt][gst]")
{
  struct Pin { const char* name; V expect; };
  static const Pin kPins[] = {
      {"RGBA", V::RGBA8}, {"BGRA", V::BGRA8}, {"ARGB", V::ARGB8}, {"ABGR", V::ABGR8},
      {"RGBx", V::RGBX8}, {"BGRx", V::BGRX8}, {"xRGB", V::XRGB8}, {"xBGR", V::XBGR8},
      {"RGB", V::RGB24},  {"BGR", V::BGR24},
      {"YUY2", V::YUYV422}, {"UYVY", V::UYVY422}, {"YVYU", V::YVYU422},
      {"v210", V::V210},  {"v216", V::V216},  {"r210", V::R210},
      {"I420", V::YUV420P}, {"Y42B", V::YUV422P}, {"Y444", V::YUV444P},
      {"Y41B", V::YUV411P}, {"YUV9", V::YUV410P},
      {"NV12", V::NV12},  {"NV21", V::NV21},  {"NV16", V::NV16},
      {"NV61", V::NV61},  {"NV24", V::NV24},
      {"I420_10LE", V::YUV420P10}, {"I422_10LE", V::YUV422P10},
      {"P010_10LE", V::P010}, {"A444", V::YUVA444P},
      {"GRAY8", V::Mono8}, {"GRAY16_LE", V::Mono16}, {"GRAY16_BE", V::Mono16BE},
  };
  for(const auto& p : kPins)
  {
    INFO("gst format " << p.name);
    CHECK(vpf::fromGStreamerFormat(p.name) == p.expect);
    CHECK(vpf::toGStreamerFormat(p.expect) == std::string_view{p.name});
  }
  // The point of a direct table: these must NOT collapse onto their U-first
  // twins, which is what routing through AVPixelFormat would have done.
  CHECK(vpf::fromGStreamerFormat("YV12") == V::YVU420P);
  CHECK(vpf::fromGStreamerFormat("YVU9") == V::YVU410P);
  CHECK(vpf::fromGStreamerFormat("YV12") != vpf::fromGStreamerFormat("I420"));
  CHECK(vpf::chromaSwappedTwin(vpf::fromGStreamerFormat("YV12"))
        == vpf::fromGStreamerFormat("I420"));
  CHECK(vpf::chromaSwappedTwin(vpf::fromGStreamerFormat("NV21"))
        == vpf::fromGStreamerFormat("NV12"));
  // Case matters in caps, and unknown names must not guess.
  CHECK(vpf::fromGStreamerFormat("nv12") == V::Unknown);
  CHECK(vpf::fromGStreamerFormat("") == V::Unknown);
  CHECK(vpf::fromGStreamerFormat("ENCODED") == V::Unknown);
  CHECK(vpf::toGStreamerFormat(V::Unknown).empty());
  // A name maps to one layout and back, for every row.
  for(const auto* i : described())
  {
    const auto name = vpf::toGStreamerFormat(i->format);
    if(name.empty())
      continue;
    INFO(i->name << " <-> " << name);
    CHECK(vpf::fromGStreamerFormat(name) == i->format);
  }
}

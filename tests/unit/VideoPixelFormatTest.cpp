// Unit tests for the video pixel-format vocabulary
// (Gfx/Graph/interop/VideoPixelFormat.{hpp,cpp}) and its libav bridge
// (VideoPixelFormatAV.{hpp,cpp}). Pure mapping logic, no app context.
//
// Covers, for EVERY enumerator in score::gfx::interop::VideoPixelFormat:
//   - descriptor invariants (plane count vs isPlanar, subsampling factors,
//     bit depth, bytesPerPrimarySample, name)
//   - stride math: alignment, tight lower bounds, hand-computed values
//     incl. 4K-odd widths and the v210 (width+47)/48*128 rule
//   - bytesPerFrame consistency against per-plane hand computation
//   - AV bridge round-trips (score -> AVPixelFormat -> score and back),
//     wire-only formats mapping to AV_PIX_FMT_NONE, unknown AV formats
//     mapping to VideoPixelFormat::Unknown
//   - cross-validation of every mapped format's descriptor against
//     FFmpeg's own av_pix_fmt_desc_get() metadata (plane count, chroma
//     subsampling, padded bits-per-pixel) — a mapping mismatch here is a
//     real bug, not a test artifact.

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace vpf = score::gfx::interop;
using V = vpf::VideoPixelFormat;

namespace
{
// Every enumerator except Unknown. Keep in sync with VideoPixelFormat.hpp;
// the "no format left behind" test below cross-checks this list against the
// documented stable numeric values.
constexpr V allKnownFormats[] = {
    // packed 8-bit RGB
    V::BGRA8, V::RGBA8, V::ARGB8, V::ABGR8, V::RGB24, V::BGR24,
    // packed 10/12-bit RGB
    V::R210, V::R12B, V::R12L, V::ARGB10, V::DPX10, V::DPX10LE, V::RGB12P,
    V::RGB48, V::RGB10,
    // packed 8-bit YUV 4:2:2
    V::UYVY422, V::YUYV422, V::YVYU422, V::VYUY422,
    // packed 10-bit YUV 4:2:2
    V::V210, V::V216,
    // planar 4:2:0
    V::NV12, V::P010, V::YUV420P, V::YUV420P10,
    // planar 4:2:2
    V::P210, V::YUV422P, V::YUV422P10,
    // planar 4:4:4
    V::YUV444P, V::YUV444P10, V::YUV444P12,
    // high-precision RGB
    V::RGBA16, V::RGBA16F, V::RGBA32F,
    // raw / bayer
    V::Mono8, V::Mono10, V::Mono12, V::Mono16, V::BayerRG8, V::BayerRG12};

constexpr std::size_t knownFormatCount
    = sizeof(allKnownFormats) / sizeof(allKnownFormats[0]);

// Documented "values are stable; reorder only with serialization migration".
// Freeze the wire values so an accidental renumbering breaks the build here.
static_assert(uint16_t(V::Unknown) == 0);
static_assert(uint16_t(V::BGRA8) == 1);
static_assert(uint16_t(V::RGBA8) == 2);
static_assert(uint16_t(V::ARGB8) == 3);
static_assert(uint16_t(V::ABGR8) == 4);
static_assert(uint16_t(V::RGB24) == 5);
static_assert(uint16_t(V::BGR24) == 6);
static_assert(uint16_t(V::R210) == 10);
static_assert(uint16_t(V::R12B) == 11);
static_assert(uint16_t(V::R12L) == 12);
static_assert(uint16_t(V::ARGB10) == 13);
static_assert(uint16_t(V::DPX10) == 14);
static_assert(uint16_t(V::DPX10LE) == 15);
static_assert(uint16_t(V::RGB12P) == 16);
static_assert(uint16_t(V::RGB48) == 17);
static_assert(uint16_t(V::RGB10) == 18);
static_assert(uint16_t(V::UYVY422) == 20);
static_assert(uint16_t(V::YUYV422) == 21);
static_assert(uint16_t(V::YVYU422) == 22);
static_assert(uint16_t(V::VYUY422) == 23);
static_assert(uint16_t(V::V210) == 30);
static_assert(uint16_t(V::V216) == 31);
static_assert(uint16_t(V::NV12) == 40);
static_assert(uint16_t(V::P010) == 41);
static_assert(uint16_t(V::YUV420P) == 42);
static_assert(uint16_t(V::YUV420P10) == 43);
static_assert(uint16_t(V::P210) == 50);
static_assert(uint16_t(V::YUV422P) == 51);
static_assert(uint16_t(V::YUV422P10) == 52);
static_assert(uint16_t(V::YUV444P) == 60);
static_assert(uint16_t(V::YUV444P10) == 61);
static_assert(uint16_t(V::YUV444P12) == 62);
static_assert(uint16_t(V::RGBA16) == 70);
static_assert(uint16_t(V::RGBA16F) == 71);
static_assert(uint16_t(V::RGBA32F) == 72);
static_assert(uint16_t(V::Mono8) == 80);
static_assert(uint16_t(V::Mono10) == 81);
static_assert(uint16_t(V::Mono12) == 82);
static_assert(uint16_t(V::Mono16) == 83);
static_assert(uint16_t(V::BayerRG8) == 84);
static_assert(uint16_t(V::BayerRG12) == 85);

// alignUp is constexpr; pin its behavior at compile time.
static_assert(vpf::alignUp(0, 256) == 0);
static_assert(vpf::alignUp(1, 256) == 256);
static_assert(vpf::alignUp(256, 256) == 256);
static_assert(vpf::alignUp(257, 256) == 512);
static_assert(vpf::alignUp(7680, 256) == 7680);
static_assert(vpf::alignUp(100, 64) == 128);
static_assert(vpf::alignUp(63, 1) == 63);

bool isKnown(V f)
{
  for(auto k : allKnownFormats)
    if(k == f)
      return true;
  return false;
}
} // namespace

TEST_CASE("every known format has a valid descriptor", "[gfx][pixfmt]")
{
  for(V f : allKnownFormats)
  {
    const auto& info = vpf::formatInfo(f);
    INFO("format " << info.name << " (" << int(uint16_t(f)) << ")");

    // Name present and not the sentinel
    REQUIRE(info.name != nullptr);
    CHECK(std::string(info.name) != "unknown");
    CHECK(std::string(info.name) == vpf::formatName(f));

    // Bit depth
    CHECK(info.bitsPerPixel > 0);
    CHECK(info.bitsPerPixel <= 128); // RGBA32F is the ceiling

    // Plane counts: 1 packed, 2 semi-planar, 3 fully planar
    CHECK(info.planeCount >= 1);
    CHECK(info.planeCount <= 3);
    CHECK(info.isPlanar == (info.planeCount >= 2));

    // Subsampling factors are 1 or 2, and vertical implies horizontal
    CHECK((info.horizontalSubsampling == 1 || info.horizontalSubsampling == 2));
    CHECK((info.verticalSubsampling == 1 || info.verticalSubsampling == 2));
    if(info.verticalSubsampling == 2)
      CHECK(info.horizontalSubsampling == 2); // 4:2:0 only; no 4:4:0 formats
    if(!info.isYuv)
    {
      // RGB / mono / bayer formats are never subsampled
      CHECK(info.horizontalSubsampling == 1);
      CHECK(info.verticalSubsampling == 1);
    }

    // Planar formats carry an explicit primary-plane sample size;
    // packed formats must have the documented 0 sentinel.
    if(info.isPlanar)
    {
      CHECK(info.bytesPerPrimarySample > 0);
      CHECK(info.bytesPerPrimarySample <= 2);
      // Sanity: average bpp = 8*s*(1 + 2/(hs*vs)) for planar YUV with
      // s bytes per sample on every plane.
      const unsigned s = info.bytesPerPrimarySample;
      const unsigned hs = info.horizontalSubsampling;
      const unsigned vs = info.verticalSubsampling;
      CHECK(unsigned(info.bitsPerPixel) == 8u * s + 16u * s / (hs * vs));
    }
    else
    {
      CHECK(info.bytesPerPrimarySample == 0);
    }

    // Stride alignment must be a nonzero power of two
    CHECK(info.defaultStrideAlignment > 0);
    CHECK((info.defaultStrideAlignment & (info.defaultStrideAlignment - 1)) == 0);

    // The reference returned is stable
    CHECK(&vpf::formatInfo(f) == &info);
  }
}

TEST_CASE("descriptor family properties", "[gfx][pixfmt]")
{
  // Spot-check each family's key fields against the header documentation.
  auto check = [](V f, uint8_t bpp, uint8_t planes, uint8_t hs, uint8_t vs,
                  bool yuv, uint8_t primary) {
    const auto& i = vpf::formatInfo(f);
    INFO("format " << i.name);
    CHECK(i.bitsPerPixel == bpp);
    CHECK(i.planeCount == planes);
    CHECK(i.horizontalSubsampling == hs);
    CHECK(i.verticalSubsampling == vs);
    CHECK(i.isYuv == yuv);
    CHECK(i.bytesPerPrimarySample == primary);
  };

  check(V::BGRA8, 32, 1, 1, 1, false, 0);
  check(V::RGB24, 24, 1, 1, 1, false, 0);
  check(V::ARGB10, 40, 1, 1, 1, false, 0); // 4x10-bit in 5 bytes (see .cpp note)
  check(V::RGB48, 48, 1, 1, 1, false, 0);
  check(V::RGB12P, 36, 1, 1, 1, false, 0); // 2px / 9 bytes
  check(V::UYVY422, 16, 1, 2, 1, true, 0);
  check(V::V210, 22, 1, 2, 1, true, 0); // effective 21.33, rounded up
  check(V::V216, 32, 1, 2, 1, true, 0);
  check(V::NV12, 12, 2, 2, 2, true, 1);
  check(V::P010, 24, 2, 2, 2, true, 2);
  check(V::P210, 32, 2, 2, 1, true, 2);
  check(V::YUV420P, 12, 3, 2, 2, true, 1);
  check(V::YUV420P10, 24, 3, 2, 2, true, 2);
  check(V::YUV422P, 16, 3, 2, 1, true, 1);
  check(V::YUV422P10, 32, 3, 2, 1, true, 2);
  check(V::YUV444P, 24, 3, 1, 1, true, 1);
  check(V::YUV444P10, 48, 3, 1, 1, true, 2);
  check(V::YUV444P12, 48, 3, 1, 1, true, 2);
  check(V::RGBA16, 64, 1, 1, 1, false, 0);
  check(V::RGBA16F, 64, 1, 1, 1, false, 0);
  check(V::RGBA32F, 128, 1, 1, 1, false, 0);
  check(V::Mono8, 8, 1, 1, 1, false, 0);
  check(V::Mono10, 16, 1, 1, 1, false, 0);  // stored in 16-bit lanes
  check(V::Mono16, 16, 1, 1, 1, false, 0);
  check(V::BayerRG8, 8, 1, 1, 1, false, 0);
  check(V::BayerRG12, 16, 1, 1, 1, false, 0);
}

TEST_CASE("unknown / out-of-range formats return the sentinel", "[gfx][pixfmt]")
{
  const auto& u = vpf::formatInfo(V::Unknown);
  CHECK(std::string(u.name) == "unknown");
  CHECK(u.bitsPerPixel == 0);
  CHECK(u.planeCount == 1);
  CHECK(!u.isYuv);
  CHECK(!u.isPlanar);
  CHECK(std::string(vpf::formatName(V::Unknown)) == "unknown");

  // Out-of-range value (e.g. deserialized from a newer score) falls through
  // to the same sentinel instead of UB.
  const auto bogus = static_cast<V>(999);
  CHECK(&vpf::formatInfo(bogus) == &u);
  CHECK(std::string(vpf::formatName(bogus)) == "unknown");

  // Size helpers return the documented 0 for unknowns
  CHECK(vpf::defaultStride(V::Unknown, 1920) == 0);
  CHECK(vpf::bytesPerFrame(V::Unknown, 1920, 1080) == 0);
  CHECK(vpf::defaultStride(bogus, 1920) == 0);
  CHECK(vpf::bytesPerFrame(bogus, 1920, 1080) == 0);
}

TEST_CASE("bytesPerFrame degenerate sizes", "[gfx][pixfmt]")
{
  for(V f : allKnownFormats)
  {
    INFO("format " << vpf::formatName(f));
    CHECK(vpf::bytesPerFrame(f, 0, 1080) == 0);
    CHECK(vpf::bytesPerFrame(f, 1920, 0) == 0);
    CHECK(vpf::bytesPerFrame(f, 0, 0) == 0);
  }
}

TEST_CASE("stride alignment and lower bounds for all formats", "[gfx][pixfmt]")
{
  const uint32_t widths[]
      = {1, 2, 6, 16, 47, 48, 49, 100, 640, 719, 720, 1280, 1919, 1920,
         1921, 3840, 3841, 4096, 4097, 7680, 8191};

  for(V f : allKnownFormats)
  {
    const auto& info = vpf::formatInfo(f);
    for(uint32_t w : widths)
    {
      INFO("format " << info.name << " width " << w);
      const std::size_t stride = vpf::defaultStride(f, w);
      CHECK(stride > 0);

      if(f == V::V210)
      {
        // SMPTE v210: 128-byte aligned, and per the documented formula.
        CHECK(stride % 128 == 0);
        CHECK(stride == ((std::size_t(w) + 47u) / 48u) * 128u);
        // Must hold at least the real packed bytes: 16 bytes per 6 pixels.
        CHECK(stride >= ((std::size_t(w) + 5u) / 6u) * 16u);
      }
      else
      {
        CHECK(stride % info.defaultStrideAlignment == 0);
        // Lower bound: a row of primary-plane samples must fit.
        const std::size_t tightRow
            = (info.isPlanar && info.bytesPerPrimarySample > 0)
                  ? std::size_t(w) * info.bytesPerPrimarySample
                  : (std::size_t(w) * info.bitsPerPixel + 7u) / 8u;
        CHECK(stride >= tightRow);
        // No runaway padding: stays within one alignment quantum.
        CHECK(stride < tightRow + info.defaultStrideAlignment);
      }
    }
  }
}

TEST_CASE("hand-computed strides: packed formats", "[gfx][pixfmt]")
{
  // BGRA8, align 256
  CHECK(vpf::defaultStride(V::BGRA8, 1920) == 7680);
  CHECK(vpf::defaultStride(V::BGRA8, 1921) == 7936);  // 7684 -> 7936
  CHECK(vpf::defaultStride(V::BGRA8, 3840) == 15360);
  CHECK(vpf::defaultStride(V::BGRA8, 3841) == 15616); // 15364 -> 15616 (4K-odd)
  CHECK(vpf::defaultStride(V::BGRA8, 1) == 256);

  // RGB24, align 64
  CHECK(vpf::defaultStride(V::RGB24, 1920) == 5760);
  CHECK(vpf::defaultStride(V::RGB24, 641) == 1984);   // 1923 -> 1984
  CHECK(vpf::defaultStride(V::RGB24, 3841) == 11584); // 11523 -> 11584

  // UYVY422, 2 bytes/px, align 256
  CHECK(vpf::defaultStride(V::UYVY422, 1920) == 3840);
  CHECK(vpf::defaultStride(V::UYVY422, 720) == 1536);  // 1440 -> 1536
  CHECK(vpf::defaultStride(V::UYVY422, 3841) == 7936); // 7682 -> 7936

  // RGB48, 6 bytes/px
  CHECK(vpf::defaultStride(V::RGB48, 1920) == 11520);
  CHECK(vpf::defaultStride(V::RGB48, 3841) == 23296); // 23046 -> 23296

  // ARGB10, 5 bytes/px (40-bit) — the regression noted in the .cpp comment:
  // treating it as 4 bytes/px undersizes DMA buffers by 20%.
  CHECK(vpf::defaultStride(V::ARGB10, 1920) == 9728); // 9600 -> 9728
  CHECK(vpf::defaultStride(V::ARGB10, 4096) == 20480);

  // RGB12P: 36 bits/px, i.e. 2 px in 9 bytes
  CHECK(vpf::defaultStride(V::RGB12P, 2) == 256);      // 9 -> 256
  CHECK(vpf::defaultStride(V::RGB12P, 1920) == 8704);  // 8640 -> 8704
  CHECK(vpf::defaultStride(V::RGB12P, 128) == 768);    // 576 -> 768

  // Mono8, align 64
  CHECK(vpf::defaultStride(V::Mono8, 100) == 128);
  CHECK(vpf::defaultStride(V::Mono8, 1920) == 1920);
  CHECK(vpf::defaultStride(V::Mono8, 3841) == 3904); // 3841 -> 3904

  // Mono10/12 stored as 16-bit lanes, align 64
  CHECK(vpf::defaultStride(V::Mono10, 1920) == 3840);
  CHECK(vpf::defaultStride(V::Mono12, 1023) == 2048); // 2046 -> 2048

  // RGBA32F, 16 bytes/px
  CHECK(vpf::defaultStride(V::RGBA32F, 1920) == 30720);
  CHECK(vpf::defaultStride(V::RGBA32F, 3841) == 61696); // 61456 -> 61696
}

TEST_CASE("hand-computed strides: v210 width rule", "[gfx][pixfmt][v210]")
{
  // v210 packs 6 pixels into 16 bytes; rows are (width+47)/48 * 128 bytes.
  // Reference values match the SMPTE/QuickTime v210 row sizes.
  CHECK(vpf::defaultStride(V::V210, 720) == 1920);   // NTSC/PAL SD
  CHECK(vpf::defaultStride(V::V210, 1280) == 3456);  // 720p
  CHECK(vpf::defaultStride(V::V210, 1920) == 5120);  // 1080
  CHECK(vpf::defaultStride(V::V210, 2048) == 5504);  // 2K DCI: (2095/48)=43 -> 43*128
  CHECK(vpf::defaultStride(V::V210, 3840) == 10240); // UHD
  CHECK(vpf::defaultStride(V::V210, 4096) == 11008); // 4K DCI
  CHECK(vpf::defaultStride(V::V210, 3841) == 10368); // 4K-odd: 81 groups

  // width%6 / 48-pixel group boundaries
  CHECK(vpf::defaultStride(V::V210, 1) == 128);
  CHECK(vpf::defaultStride(V::V210, 47) == 128);
  CHECK(vpf::defaultStride(V::V210, 48) == 128);
  CHECK(vpf::defaultStride(V::V210, 49) == 256);
  CHECK(vpf::defaultStride(V::V210, 96) == 256);
  CHECK(vpf::defaultStride(V::V210, 97) == 384);
}

TEST_CASE("hand-computed frame sizes: packed", "[gfx][pixfmt]")
{
  // packed: bytesPerFrame == stride * height
  CHECK(vpf::bytesPerFrame(V::BGRA8, 1920, 1080) == 7680u * 1080u);
  CHECK(vpf::bytesPerFrame(V::UYVY422, 1920, 1080) == 3840u * 1080u);
  CHECK(vpf::bytesPerFrame(V::UYVY422, 720, 486) == 1536u * 486u);
  CHECK(vpf::bytesPerFrame(V::V210, 1920, 1080) == 5120u * 1080u);
  CHECK(vpf::bytesPerFrame(V::V210, 3841, 2161) == 10368u * 2161u);
  CHECK(vpf::bytesPerFrame(V::RGBA32F, 3840, 2160) == 61440u * 2160u);
  CHECK(vpf::bytesPerFrame(V::Mono8, 100, 7) == 128u * 7u);

  for(V f : allKnownFormats)
  {
    const auto& info = vpf::formatInfo(f);
    if(info.isPlanar)
      continue;
    INFO("format " << info.name);
    CHECK(vpf::bytesPerFrame(f, 1921, 1081)
          == vpf::defaultStride(f, 1921) * 1081u);
  }
}

TEST_CASE("hand-computed frame sizes: planar and semi-planar", "[gfx][pixfmt]")
{
  // NV12 1920x1080: rows are 1920 bytes but align to 256 -> 2048.
  // Y 2048x1080 + interleaved UV 2048x540 (1-byte samples).
  CHECK(vpf::bytesPerFrame(V::NV12, 1920, 1080)
        == 2048u * 1080u + 2048u * 540u); // 3317760 (padded 1.5x)
  // P010: same shape, 2-byte lanes
  CHECK(vpf::bytesPerFrame(V::P010, 1920, 1080)
        == 3840u * 1080u + 3840u * 540u); // 6220800
  // P210: 4:2:2 semi-planar, UV plane is full height
  CHECK(vpf::bytesPerFrame(V::P210, 1920, 1080)
        == 3840u * 1080u + 3840u * 1080u);

  // YUV420P 1920x1080: luma rows 1920 -> 2048; chroma rows 960 -> 1024
  CHECK(vpf::bytesPerFrame(V::YUV420P, 1920, 1080)
        == 2048u * 1080u + 2u * 1024u * 540u);
  // YUV420P10 3840x2160: luma rows 7680, chroma rows 3840 (aligned already)
  CHECK(vpf::bytesPerFrame(V::YUV420P10, 3840, 2160)
        == 7680u * 2160u + 2u * 3840u * 1080u); // 24883200
  // YUV422P: chroma planes full height (luma 2048, chroma 1024)
  CHECK(vpf::bytesPerFrame(V::YUV422P, 1920, 1080)
        == 2048u * 1080u + 2u * 1024u * 1080u);
  // YUV444P: three equal planes, rows padded 1920 -> 2048
  CHECK(vpf::bytesPerFrame(V::YUV444P, 1920, 1080) == 3u * 2048u * 1080u);
  CHECK(vpf::bytesPerFrame(V::YUV444P12, 1920, 1080) == 3u * 3840u * 1080u);

  // 4K-odd size: NV12 3841x2161.
  // Y stride alignUp(3841,256)=4096; cWidth=1920, cHeight=1080,
  // UV stride = defaultStride(3840)=3840.
  CHECK(vpf::bytesPerFrame(V::NV12, 3841, 2161)
        == 4096u * 2161u + 3840u * 1080u);
  // YUV420P10 3841x2161: Y stride alignUp(7682,256)=7936;
  // chroma stride = defaultStride(1920) = 3840 (2-byte samples).
  CHECK(vpf::bytesPerFrame(V::YUV420P10, 3841, 2161)
        == 7936u * 2161u + 2u * 3840u * 1080u);

  // Generic consistency: planar frame size decomposes into the same
  // per-plane arithmetic the implementation documents.
  for(V f : allKnownFormats)
  {
    const auto& info = vpf::formatInfo(f);
    if(!info.isPlanar)
      continue;
    INFO("format " << info.name);
    const uint32_t w = 1921, h = 1081;
    const std::size_t y = vpf::defaultStride(f, w) * h;
    const std::size_t cw = w / info.horizontalSubsampling;
    const std::size_t ch = h / info.verticalSubsampling;
    std::size_t expected{};
    if(info.planeCount == 2)
      expected = y + vpf::defaultStride(f, uint32_t(cw * 2)) * ch;
    else
      expected = y + 2u * vpf::defaultStride(f, uint32_t(cw)) * ch;
    CHECK(vpf::bytesPerFrame(f, w, h) == expected);

    // And the frame is at least as big as the tightly-packed payload.
    const std::size_t tight
        = (std::size_t(w) * h * info.bitsPerPixel) / 8u;
    CHECK(vpf::bytesPerFrame(f, w, h) >= tight - info.bitsPerPixel);
  }
}

TEST_CASE("format names are unique across the vocabulary", "[gfx][pixfmt]")
{
  std::set<std::string> names;
  for(V f : allKnownFormats)
    names.insert(vpf::formatName(f));
  CHECK(names.size() == knownFormatCount);
}

// ---------------------------------------------------------------------------
// libav bridge
// ---------------------------------------------------------------------------

namespace
{
// Formats documented in VideoPixelFormatAV as wire-only / no AV twin.
constexpr V wireOnlyFormats[] = {
    V::V210, V::V216, V::R210, V::RGB10, V::R12B, V::R12L, V::ARGB10,
    V::DPX10, V::DPX10LE, V::RGB12P, V::VYUY422, V::RGBA16F, V::RGBA32F,
    V::BayerRG8, V::BayerRG12};

bool isWireOnly(V f)
{
  for(auto k : wireOnlyFormats)
    if(k == f)
      return true;
  return false;
}
} // namespace

TEST_CASE("AV bridge: score -> AV -> score round-trip", "[gfx][pixfmt][av]")
{
  for(V f : allKnownFormats)
  {
    INFO("format " << vpf::formatName(f));
    const AVPixelFormat av = vpf::toAVPixelFormat(f);
    if(isWireOnly(f))
    {
      CHECK(av == AV_PIX_FMT_NONE);
    }
    else
    {
      REQUIRE(av != AV_PIX_FMT_NONE);
      CHECK(vpf::fromAVPixelFormat(av) == f);
    }
  }
  CHECK(vpf::toAVPixelFormat(V::Unknown) == AV_PIX_FMT_NONE);
  // Out-of-range values (future formats from a newer score) hit the
  // post-switch fallback instead of UB.
  CHECK(vpf::toAVPixelFormat(static_cast<V>(999)) == AV_PIX_FMT_NONE);
}

TEST_CASE("AV bridge: AV -> score -> AV round-trip", "[gfx][pixfmt][av]")
{
  // Every AVPixelFormat the bridge accepts must map back to itself.
  const AVPixelFormat mapped[] = {
      AV_PIX_FMT_BGRA,        AV_PIX_FMT_RGBA,       AV_PIX_FMT_ARGB,
      AV_PIX_FMT_ABGR,        AV_PIX_FMT_RGB24,      AV_PIX_FMT_BGR24,
      AV_PIX_FMT_RGB48LE,     AV_PIX_FMT_RGBA64LE,   AV_PIX_FMT_UYVY422,
      AV_PIX_FMT_YUYV422,     AV_PIX_FMT_YVYU422,    AV_PIX_FMT_NV12,
      AV_PIX_FMT_P010LE,      AV_PIX_FMT_P210LE,     AV_PIX_FMT_YUV420P,
      AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV422P,    AV_PIX_FMT_YUV422P10LE,
      AV_PIX_FMT_YUV444P,     AV_PIX_FMT_YUV444P10LE, AV_PIX_FMT_YUV444P12LE,
      AV_PIX_FMT_GRAY8,       AV_PIX_FMT_GRAY10LE,   AV_PIX_FMT_GRAY12LE,
      AV_PIX_FMT_GRAY16LE};

  std::set<V> seen;
  for(AVPixelFormat av : mapped)
  {
    INFO("AVPixelFormat " << av_get_pix_fmt_name(av));
    const V f = vpf::fromAVPixelFormat(av);
    REQUIRE(f != V::Unknown);
    CHECK(isKnown(f));
    CHECK(vpf::toAVPixelFormat(f) == av);
    seen.insert(f); // each AV format maps to a distinct wire format
  }
  CHECK(seen.size() == sizeof(mapped) / sizeof(mapped[0]));
}

TEST_CASE("AV bridge: unmapped AV formats return Unknown", "[gfx][pixfmt][av]")
{
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_NONE) == V::Unknown);
  // Common AVPixelFormats deliberately outside the wire-format vocabulary
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_GBRP) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_GBRP10LE) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_YUV410P) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_YUVA420P) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_PAL8) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_RGB565LE) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_VAAPI) == V::Unknown);
  // Big-endian twins of mapped little-endian formats must NOT round-trip
  // into the LE wire formats.
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_RGB48BE) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_YUV420P10BE) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_GRAY16BE) == V::Unknown);
  CHECK(vpf::fromAVPixelFormat(AV_PIX_FMT_P010BE) == V::Unknown);
}

TEST_CASE("descriptors agree with FFmpeg's pixdesc for every mapped format",
          "[gfx][pixfmt][av]")
{
  // For each format with an AV twin, our descriptor must agree with
  // FFmpeg's authoritative metadata. A failure here is a real mapping bug.
  for(V f : allKnownFormats)
  {
    const AVPixelFormat av = vpf::toAVPixelFormat(f);
    if(av == AV_PIX_FMT_NONE)
      continue;

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(av);
    REQUIRE(desc != nullptr);
    const auto& info = vpf::formatInfo(f);
    INFO("format " << info.name << " <-> " << desc->name);

    // Plane count
    CHECK(int(info.planeCount) == av_pix_fmt_count_planes(av));

    // Chroma subsampling
    CHECK(int(info.horizontalSubsampling) == (1 << desc->log2_chroma_w));
    CHECK(int(info.verticalSubsampling) == (1 << desc->log2_chroma_h));

    // Average bits per pixel including padding lanes (P010 16-bit lanes etc.)
    CHECK(int(info.bitsPerPixel) == av_get_padded_bits_per_pixel(desc));

    // YUV-ness: 3+ components without the RGB flag
    const bool avIsYuv
        = desc->nb_components >= 3 && !(desc->flags & AV_PIX_FMT_FLAG_RGB);
    CHECK(info.isYuv == avIsYuv);

    // Planar-ness: multiple data planes on the AV side too
    CHECK(info.isPlanar == (av_pix_fmt_count_planes(av) >= 2));

    // Primary (luma / first) plane sample size must match FFmpeg's step
    if(info.isPlanar)
      CHECK(int(info.bytesPerPrimarySample) == desc->comp[0].step);

    // Our default stride must be able to hold FFmpeg's linesize for the
    // primary plane at various widths (incl. 4K-odd).
    for(uint32_t w : {720u, 1920u, 3841u, 4096u})
    {
      const int avLinesize = av_image_get_linesize(av, int(w), 0);
      REQUIRE(avLinesize > 0);
      CHECK(vpf::defaultStride(f, w) >= std::size_t(avLinesize));
    }
  }
}

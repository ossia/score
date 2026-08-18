// Unit tests for makeWireDecoder (Gfx/Graph/decoders/WireDecoderFactory.hpp),
// the capture-side mirror of makeWireEncoder.
//
// The decoder factory is what a capture-card backend calls to turn "the pixel
// format the card puts on the wire" into something that unpacks those bytes at
// sample time. A format the vocabulary describes, the card enumerates and the
// factory has no case for is accepted at negotiation and then renders nothing --
// that has already happened twice on this path (YVYU422/VYUY422, and the six
// planar layouts below).
//
// The sweep iterates allFormats(), i.e. the declarative table the vocabulary is
// generated from, and compares against ONE hand-written list: the formats the
// factory claims. A format that gains or loses a decoder without that list
// moving is a failure that names the format.
//
// No QRhi and no application: the decoders are constructed, not initialised.

#include <Gfx/Graph/decoders/WireDecoderFactory.hpp>
#include <Gfx/Graph/encoders/WireEncoderFactory.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

namespace vpf = score::gfx::interop;
using V = vpf::VideoPixelFormat;
using score::gfx::makeWireDecoder;
using score::gfx::makeWireEncoder;

namespace
{
std::vector<const vpf::VideoPixelFormatInfo*> described()
{
  std::size_t n = 0;
  const auto* p = vpf::allFormats(n);
  std::vector<const vpf::VideoPixelFormatInfo*> v;
  v.reserve(n);
  for(std::size_t i = 0; i < n; ++i)
    v.push_back(p + i);
  return v;
}

// Every format WireDecoderFactory.hpp has a case for, in the order the switch
// lists them.
const std::set<V>& decodable()
{
  static const std::set<V> s{
      // packed 8-bit YUV 4:2:2, including the chroma-swapped twins
      V::UYVY422, V::YUYV422, V::YVYU422, V::VYUY422,
      // packed 10-bit YUV 4:2:2
      V::V210,
      // packed 8-bit RGB, including the alpha-pinned X variants
      V::BGRA8, V::RGBA8, V::BGRX8, V::RGBX8, V::ARGB8, V::XRGB8, V::ABGR8,
      V::XBGR8,
      // greyscale
      V::Mono8, V::Mono10, V::Mono12, V::Mono16, V::Mono16BE,
      // colour-filter-array
      V::BayerRGGB8, V::BayerRG8, V::BayerBGGR8, V::BayerGRBG8, V::BayerGBRG8,
      V::BayerRGGB16, V::BayerBGGR16, V::BayerRGGB10, V::BayerBGGR10,
      V::BayerGRBG10, V::BayerGBRG10, V::BayerRG12,
      // sub-byte packed RGB / YUV
      V::RGB332, V::RGB565, V::RGB565BE, V::RGB555, V::RGB555BE, V::ARGB1555,
      V::RGB444, V::ARGB4444, V::AYUV4444, V::AYUV1555, V::YUV565,
      // packed RGB 10/24/48
      V::R210, V::RGB24, V::BGR24, V::RGB48,
      // planar and semi-planar
      V::NV12, V::NV21, V::NV16, V::NV61, V::NV24, V::NV42, V::VUYA, V::VUYX,
      V::AYUV, V::XYUV, V::YUVA, V::YUVX, V::YUV420P, V::YVU420P, V::YUV422P,
      V::P010, V::YUV422P10, V::YUV422P12, V::YUV420P10, V::YUV444P,
      V::YUV444P10, V::YUV444P12, V::P210};
  return s;
}
} // namespace

TEST_CASE("every wire format either has a decoder or is named", "[gfx][decoders]")
{
  Video::ImageFormat meta;
  for(const auto* i : described())
  {
    INFO("format " << i->name);
    CHECK((makeWireDecoder(i->format, meta) != nullptr)
          == (decodable().count(i->format) == 1));
  }
  CHECK(makeWireDecoder(V::Unknown, meta) == nullptr);
}

// The six planar layouts wired up last, plus the two chroma-swapped packed
// formats that were enumerated as supported and rendered nothing. Pinned by
// name rather than left to the sweep: the sweep only proves the factory and the
// list agree, and both were wrong at the same time.
TEST_CASE("the late planar and chroma-swapped decoders exist", "[gfx][decoders]")
{
  Video::ImageFormat meta;
  const V late[] = {V::P210,     V::YUV420P10, V::YUV422P12, V::YUV444P,
                    V::YUV444P10, V::YUV444P12, V::YVYU422,  V::VYUY422};
  for(V f : late)
  {
    INFO("format " << vpf::formatName(f));
    CHECK(makeWireDecoder(f, meta) != nullptr);
  }
}

// A card that can be fed a format can normally also deliver it. The exceptions
// are the packed high-bit-depth RGB layouts score encodes for SDI playout and
// has never had to unpack; they are named so the list cannot grow silently.
TEST_CASE("every encodable format is also decodable", "[gfx][decoders]")
{
  static const std::set<V> kEncodeOnly{V::RGB10, V::ARGB10, V::DPX10,
                                       V::DPX10LE, V::RGB12P};
  Video::ImageFormat meta;
  for(const auto* i : described())
  {
    if(makeWireEncoder(i->format) == nullptr)
      continue;
    INFO(i->name << " has an encoder");
    CHECK(
        (makeWireDecoder(i->format, meta) != nullptr)
        == (kEncodeOnly.count(i->format) == 0));
  }
}

#include <Gfx/Graph/interop/DirectShowPixelFormat.hpp>

namespace score::gfx::interop
{
namespace
{
constexpr auto fcc = directShowFourcc;
}

VideoPixelFormat fromDirectShowFourcc(uint32_t fourcc) noexcept
{
  using V = VideoPixelFormat;

  // -- packed 4:2:2, 8-bit --
  if(fourcc == fcc('Y', 'U', 'Y', '2') || fourcc == fcc('Y', 'U', 'Y', 'V'))
    return V::YUYV422;
  if(fourcc == fcc('U', 'Y', 'V', 'Y') || fourcc == fcc('Y', '4', '2', '2'))
    return V::UYVY422;
  if(fourcc == fcc('Y', 'V', 'Y', 'U'))
    return V::YVYU422;

  // -- packed 4:2:2, 10 and 16-bit. Y210/Y216 carry YUYV component order;
  //    V216 differs from Y216 only in that order, and conflating the two was
  //    the reason the old mapping carried a "not sure" note.
  if(fourcc == fcc('Y', '2', '1', '0'))
    return V::Y210;
  if(fourcc == fcc('Y', '2', '1', '6'))
    return V::Y216;
  if(fourcc == fcc('V', '2', '1', '6'))
    return V::V216;

  // -- planar 4:2:0. YV12 stores V before U, so it is YVU420P: naming it
  //    YUV420P exchanges red and blue.
  if(fourcc == fcc('I', '4', '2', '0') || fourcc == fcc('I', 'Y', 'U', 'V'))
    return V::YUV420P;
  if(fourcc == fcc('Y', 'V', '1', '2'))
    return V::YVU420P;

  // -- planar 4:2:2 and 4:1:0, likewise V-before-U --
  if(fourcc == fcc('Y', 'V', '1', '6'))
    return V::YVU422P;
  if(fourcc == fcc('Y', 'V', 'U', '9'))
    return V::YVU410P;

  // -- semi-planar. P208/P408 are the 8-bit 4:2:2 and 4:4:4 spellings.
  if(fourcc == fcc('N', 'V', '1', '2'))
    return V::NV12;
  if(fourcc == fcc('N', 'V', '2', '1'))
    return V::NV21;
  if(fourcc == fcc('N', 'V', '1', '6') || fourcc == fcc('P', '2', '0', '8'))
    return V::NV16;
  if(fourcc == fcc('N', 'V', '2', '4') || fourcc == fcc('P', '4', '0', '8'))
    return V::NV24;
  if(fourcc == fcc('N', 'V', '4', '2'))
    return V::NV42;
  if(fourcc == fcc('P', '0', '1', '0'))
    return V::P010;
  if(fourcc == fcc('P', '2', '1', '0'))
    return V::P210;
  if(fourcc == fcc('P', '2', '1', '6'))
    return V::P216;

  // -- packed 4:4:4. DirectShow AYUV puts V,U,Y,A in memory, which is what
  //    FFmpeg calls VUYA; Y410 is 2 padding bits plus three 10-bit components,
  //    Y416 the same geometry at 16 bits with real alpha.
  if(fourcc == fcc('A', 'Y', 'U', 'V'))
    return V::VUYA;
  if(fourcc == fcc('Y', '4', '1', '0'))
    return V::XV30;
  if(fourcc == fcc('Y', '4', '1', '6'))
    return V::AYUV64;

  // -- packed 4:1:1 --
  if(fourcc == fcc('Y', '4', '1', 'P'))
    return V::UYYVYY411;

  // -- single channel. Z16 and Y16 are depth and luminance respectively, but
  //    both are a 16-bit single channel as far as the layout goes.
  if(fourcc == fcc('G', 'R', 'E', 'Y') || fourcc == fcc('Y', '8', ' ', ' ')
     || fourcc == fcc('Y', '8', '0', '0'))
    return V::Mono8;
  if(fourcc == fcc('Y', '1', '6', '0') || fourcc == fcc('Z', '1', '6', ' '))
    return V::Mono16;

  return V::Unknown;
}

bool isDirectShowCompressedFourcc(uint32_t fourcc) noexcept
{
  // MJPG and the three historical Motion-JPEG spellings DirectShow still
  // reports. These are codecs, so they must not resolve to a pixel format:
  // handing raw JPEG bytes to a planar-YUV path renders noise.
  return fourcc == fcc('M', 'J', 'P', 'G') || fourcc == fcc('T', 'V', 'M', 'J')
         || fourcc == fcc('W', 'A', 'K', 'E') || fourcc == fcc('P', 'l', 'u', 'm')
         || fourcc == fcc('d', 'v', 's', 'd') || fourcc == fcc('H', '2', '6', '4');
}

} // namespace score::gfx::interop

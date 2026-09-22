#include <Gfx/Graph/interop/DirectShowPixelFormat.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx::interop
{
namespace
{
constexpr auto fcc = directShowFourcc;
}

Video::VideoPixelFormat fromDirectShowFourcc(uint32_t fourcc) noexcept
{
  using V = Video::VideoPixelFormat;

  // -- packed 4:2:2, 8-bit. The aliases are the ones FFmpeg's raw table
  //    (libavcodec/raw_pix_fmt_tags.h) carries, so ffmpeg's dshow resolves a
  //    pin spelt any of these ways to the same layout we offer it under.
  if(fourcc == fcc('Y', 'U', 'Y', '2') || fourcc == fcc('Y', 'U', 'Y', 'V')
     || fourcc == fcc('Y', 'U', 'N', 'V') || fourcc == fcc('V', '4', '2', '2')
     || fourcc == fcc('V', 'Y', 'U', 'Y') || fourcc == fcc('y', 'u', 'v', 's')
     || fourcc == fcc('y', 'u', 'v', '2'))
    return V::YUYV422;
  //    HDYC is UYVY carrying BT.709 primaries, as capture cards spell it.
  if(fourcc == fcc('U', 'Y', 'V', 'Y') || fourcc == fcc('Y', '4', '2', '2')
     || fourcc == fcc('H', 'D', 'Y', 'C') || fourcc == fcc('U', 'Y', 'N', 'V')
     || fourcc == fcc('U', 'Y', 'N', 'Y') || fourcc == fcc('u', 'y', 'v', '1')
     || fourcc == fcc('2', 'V', 'u', '1') || fourcc == fcc('2', 'v', 'u', 'y'))
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

  // -- planar 4:2:2, 4:1:1 and 4:1:0. YUV9 is the U-before-V twin of YVU9;
  //    Y42B/P422 and Y41B are the AVI spellings of the U-before-V layouts.
  if(fourcc == fcc('Y', 'V', '1', '6'))
    return V::YVU422P;
  if(fourcc == fcc('Y', '4', '2', 'B') || fourcc == fcc('P', '4', '2', '2'))
    return V::YUV422P;
  if(fourcc == fcc('Y', '4', '1', 'B'))
    return V::YUV411P;
  if(fourcc == fcc('Y', 'V', 'U', '9'))
    return V::YVU410P;
  if(fourcc == fcc('Y', 'U', 'V', '9'))
    return V::YUV410P;

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

  // -- packed 4:1:1. Y411 and Y41P are one U0 Y0 V0 Y1 ... macropixel under
  //    two names.
  if(fourcc == fcc('Y', '4', '1', 'P') || fourcc == fcc('Y', '4', '1', '1'))
    return V::UYYVYY411;

  // -- single channel. Z16 and Y16 are depth and luminance respectively, but
  //    both are a 16-bit single channel as far as the layout goes. 'Y16 ' is
  //    the spelling dshow.h and the KS drivers use, and the one GStreamer's
  //    winks source special-cases.
  if(fourcc == fcc('G', 'R', 'E', 'Y') || fourcc == fcc('Y', '8', ' ', ' ')
     || fourcc == fcc('Y', '8', '0', '0'))
    return V::Mono8;
  if(fourcc == fcc('Y', '1', '6', '0') || fourcc == fcc('Y', '1', '6', ' ')
     || fourcc == fcc('Z', '1', '6', ' '))
    return V::Mono16;

  return V::Unknown;
}

namespace
{
/// The spellings a DirectShow filter presents that FFmpeg's RIFF video tags do
/// not carry.
AVCodecID directShowOnlyCodec(uint32_t fourcc) noexcept
{
  // H.265 has two spellings in the wild: UVC cameras label the pin 'H265'
  // ({35363248-0000-0010-8000-...}), OBS and the LAV filters 'HEVC'
  // ({43564548-...}).
  if(fourcc == fcc('H', 'E', 'V', 'C') || fourcc == fcc('H', '2', '6', '5')
     || fourcc == fcc('h', 'v', 'c', '1') || fourcc == fcc('h', 'e', 'v', '1'))
    return AV_CODEC_ID_HEVC;

  // The Motion-JPEG spellings dshow.h lists under "Miscellaneous Video
  // Subtypes". MJPG and IJPG come from FFmpeg's table instead.
  if(fourcc == fcc('T', 'V', 'M', 'J') || fourcc == fcc('W', 'A', 'K', 'E')
     || fourcc == fcc('C', 'F', 'C', 'C') || fourcc == fcc('P', 'l', 'u', 'm'))
    return AV_CODEC_ID_MJPEG;

  // MEDIASUBTYPE_MDVF; dvsd / dvhd / dvsl / DVCS come from FFmpeg's table.
  if(fourcc == fcc('M', 'D', 'V', 'F'))
    return AV_CODEC_ID_DVVIDEO;

  return AV_CODEC_ID_NONE;
}
}

AVCodecID directShowFourccCodec(uint32_t fourcc) noexcept
{
  if(fourcc == 0)
    return AV_CODEC_ID_NONE;

  // The layout table answers first: it is the one carrying the chroma-swap
  // decisions, and a fourcc it names is raw whatever else claims it. FFmpeg
  // calls Y41P a codec, for instance, where this unpacks it.
  if(fromDirectShowFourcc(fourcc) != Video::VideoPixelFormat::Unknown)
    return AV_CODEC_ID_RAWVIDEO;

  if(const auto id = directShowOnlyCodec(fourcc); id != AV_CODEC_ID_NONE)
    return id;

  // Everything else: the fourcc -> codec table FFmpeg maintains for RIFF/AVI,
  // which is where a DirectShow pin's fourccs come from too, and which
  // ffmpeg's own dshow device resolves them through. H.264's eight spellings,
  // DV, VP8/VP9/AV1, MPEG-4/DIVX/Xvid, H.263, JPEG 2000 and v210 all come from
  // there, with no list here to keep in sync.
  const AVCodecTag* const tags[] = {avformat_get_riff_video_tags(), nullptr};
  const auto id = av_codec_get_id(tags, fourcc);

  // RAWVIDEO from that table is a layout the one above could not name, which
  // the capture path has no use for.
  return id == AV_CODEC_ID_RAWVIDEO ? AV_CODEC_ID_NONE : id;
}

bool isDirectShowCompressedFourcc(uint32_t fourcc) noexcept
{
  // Compressed means it resolves to a decoder: JPEG or H.264 bytes handed to
  // a planar-YUV path render noise.
  const auto id = directShowFourccCodec(fourcc);
  return id != AV_CODEC_ID_NONE && id != AV_CODEC_ID_RAWVIDEO;
}

} // namespace score::gfx::interop

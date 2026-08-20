// Video/GpuFormats.hpp: the hardware-decoding vocabulary the libav decoder
// consults before it opens a codec, and the CPU-rescale gate that decides
// whether a decoded frame reaches the GPU as-is or goes through swscale.
//
// The header is a family of pure functions of an AVPixelFormat / AVCodecID, so
// the whole decision tree is checkable with no codec, no device and no GPU --
// only hardwareDecoderIsAvailable() reads the local ffmpeg build, and every
// assertion below is written against what it answers here rather than against
// a hard-coded expectation.
//
// The assertion with teeth is the last one. formatNeedsDecoding() and
// score::gfx::createGPUVideoDecoder() are two independently maintained tables
// that MUST agree: a format the rescale gate lets through untouched and the
// GPU factory has no decoder for renders nothing at all. Each table is
// self-consistent, so nothing else can catch a disagreement.

#include <Gfx/Graph/decoders/GPUVideoDecoderFactory.hpp>
#include <Video/GpuFormats.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

using namespace Video;

namespace
{
// Every AVPixelFormat this ffmpeg build knows about, enumerated rather than
// listed: a format added by an ffmpeg upgrade must enter the sweep on its own.
std::vector<AVPixelFormat> allPixelFormats()
{
  std::vector<AVPixelFormat> out;
  const AVPixFmtDescriptor* d = nullptr;
  while((d = av_pix_fmt_desc_next(d)))
    out.push_back(av_pix_fmt_desc_get_id(d));
  return out;
}

const char* name(AVPixelFormat f)
{
  const char* n = av_get_pix_fmt_name(f);
  return n ? n : "?";
}

bool hasGpuDecoder(AVPixelFormat f)
{
  Video::ImageFormat fmt;
  fmt.width = 64;
  fmt.height = 64;
  fmt.pixel_format = f;
  return score::gfx::createGPUVideoDecoder(fmt) != nullptr;
}

// The two hosts where the cross-table check below is known to disagree; see the
// [!shouldfail] case that owns them.
bool isKnownUncoveredByFactory(AVPixelFormat f)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(60, 8, 100)
  if(f == AV_PIX_FMT_GRAYF16BE)
    return true;
#endif
  return f == AV_PIX_FMT_GRAYF32BE;
}

const AVPixelFormat kHardwareFormats[] = {
    AV_PIX_FMT_VAAPI,     AV_PIX_FMT_VDPAU, AV_PIX_FMT_DXVA2_VLD,
    AV_PIX_FMT_D3D11,     AV_PIX_FMT_CUDA,  AV_PIX_FMT_QSV,
    AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_DRM_PRIME, AV_PIX_FMT_VULKAN};
} // namespace

TEST_CASE("the hardware pixel formats are exactly the ones that skip swscale",
          "[video][gpuformats]")
{
  // formatNeedsDecoding() opens with `if(formatIsHardwareDecoded(fmt)) return
  // false` -- an opaque HW surface can never be fed to sws_scale, so the
  // implication must hold for every format, not just the listed ones.
  for(AVPixelFormat f : allPixelFormats())
  {
    INFO("format " << name(f));
    if(formatIsHardwareDecoded(f))
      CHECK_FALSE(formatNeedsDecoding(f));
  }

  for(AVPixelFormat f : kHardwareFormats)
  {
    INFO("format " << name(f));
    CHECK(formatIsHardwareDecoded(f));
  }

  // ...and a plain software layout must never be mistaken for one.
  CHECK_FALSE(formatIsHardwareDecoded(AV_PIX_FMT_YUV420P));
  CHECK_FALSE(formatIsHardwareDecoded(AV_PIX_FMT_RGBA));
  CHECK_FALSE(formatIsHardwareDecoded(AV_PIX_FMT_NV12));
  CHECK_FALSE(formatIsHardwareDecoded(AV_PIX_FMT_NONE));
}

TEST_CASE("hardwareDecoderIsAvailable only answers for hardware formats",
          "[video][gpuformats]")
{
  for(AVPixelFormat f : allPixelFormats())
  {
    INFO("format " << name(f));
    if(!formatIsHardwareDecoded(f))
      CHECK_FALSE(hardwareDecoderIsAvailable(f));
  }
  CHECK_FALSE(hardwareDecoderIsAvailable(AV_PIX_FMT_NONE));
}

TEST_CASE("ffmpegHardwareDecodingFormats agrees with availability",
          "[video][gpuformats]")
{
  // The struct is the only bridge between "score wants this acceleration" and
  // "ffmpeg can open a device for it": an entry that names a device type while
  // hardwareDecoderIsAvailable() says no would make get_format() hand the codec
  // a pixel format nothing can map.
  for(AVPixelFormat f : allPixelFormats())
  {
    INFO("format " << name(f));
    const auto info = ffmpegHardwareDecodingFormats(f);
    if(info.device == AV_HWDEVICE_TYPE_NONE)
    {
      CHECK(info.format == AV_PIX_FMT_NONE);
    }
    else
    {
      CHECK(info.format == f);
      CHECK(formatIsHardwareDecoded(f));
      CHECK(hardwareDecoderIsAvailable(f));
      // The device type must be one libavutil actually knows.
      CHECK(av_hwdevice_get_type_name(info.device) != nullptr);
    }
  }

  CHECK(ffmpegHardwareDecodingFormats(AV_PIX_FMT_YUV420P).format == AV_PIX_FMT_NONE);
  CHECK(ffmpegHardwareDecodingFormats(AV_PIX_FMT_NONE).device
        == AV_HWDEVICE_TYPE_NONE);
}

TEST_CASE("hwCodecName suffixes per device family", "[video][gpuformats]")
{
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_CUDA) == "h264_cuvid");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_QSV) == "h264_qsv");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_VDPAU) == "h264_vdpau");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_DRM) == "h264_v4l2m2m");

  // The hw_device_ctx families reuse the generic decoder: returning a suffixed
  // name for them would send codecSupportsHWPixelFormat() looking for a decoder
  // that does not exist and disable the acceleration outright.
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_VAAPI) == "h264");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_DXVA2) == "h264");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_D3D11VA) == "h264");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_VIDEOTOOLBOX) == "h264");
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_VULKAN) == "h264");

  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_NONE).empty());
  CHECK(hwCodecName("h264", AV_HWDEVICE_TYPE_OPENCL).empty());
}

TEST_CASE("the codecs score offers hardware decoding for", "[video][gpuformats]")
{
  const AVCodecID accelerable[] = {
      AV_CODEC_ID_AV1,        AV_CODEC_ID_H264,       AV_CODEC_ID_HEVC,
      AV_CODEC_ID_MJPEG,      AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO,
      AV_CODEC_ID_MPEG4,      AV_CODEC_ID_VC1,        AV_CODEC_ID_VP8,
      AV_CODEC_ID_VP9,        AV_CODEC_ID_WMV1,       AV_CODEC_ID_WMV2,
      AV_CODEC_ID_WMV3,       AV_CODEC_ID_PRORES};
  for(AVCodecID id : accelerable)
  {
    INFO("codec " << avcodec_get_name(id));
    CHECK(ffmpegCanDoHardwareDecoding(id));
  }

  // Intra-only / lossless codecs no GPU block decodes.
  CHECK_FALSE(ffmpegCanDoHardwareDecoding(AV_CODEC_ID_NONE));
  CHECK_FALSE(ffmpegCanDoHardwareDecoding(AV_CODEC_ID_RAWVIDEO));
  CHECK_FALSE(ffmpegCanDoHardwareDecoding(AV_CODEC_ID_FFV1));
  CHECK_FALSE(ffmpegCanDoHardwareDecoding(AV_CODEC_ID_HAP));
  CHECK_FALSE(ffmpegCanDoHardwareDecoding(AV_CODEC_ID_PNG));

  // Vulkan decodes these two with compute shaders rather than Vulkan Video and
  // must not be selected for them, however "accelerable" they otherwise are.
  CHECK(isVulkanComputeCodec(AV_CODEC_ID_PRORES));
  CHECK(isVulkanComputeCodec(AV_CODEC_ID_FFV1));
  CHECK_FALSE(isVulkanComputeCodec(AV_CODEC_ID_H264));
  CHECK_FALSE(isVulkanComputeCodec(AV_CODEC_ID_HEVC));
  CHECK_FALSE(isVulkanComputeCodec(AV_CODEC_ID_NONE));
}

TEST_CASE("codecSupportsHWPixelFormat rejects what cannot be accelerated",
          "[video][gpuformats]")
{
  // A software pixel format is never a hardware config.
  CHECK_FALSE(codecSupportsHWPixelFormat(AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P));
  CHECK_FALSE(codecSupportsHWPixelFormat(AV_CODEC_ID_H264, AV_PIX_FMT_NONE));
  // ...and a codec ffmpeg has no decoder for cannot support one either.
  CHECK_FALSE(codecSupportsHWPixelFormat(AV_CODEC_ID_NONE, AV_PIX_FMT_VAAPI));

  // Whatever it does answer must be consistent with the availability probe.
  for(AVPixelFormat f : kHardwareFormats)
  {
    INFO("format " << name(f));
    if(codecSupportsHWPixelFormat(AV_CODEC_ID_H264, f))
      CHECK(hardwareDecoderIsAvailable(f));
  }
}

TEST_CASE("selectHardwareAccelerations only returns usable rungs",
          "[video][gpuformats]")
{
  // graphicsApi: 0=Null 1=OpenGL 2=Vulkan 3=D3D11 4=Metal 5=D3D12
  const uint32_t vendors[] = {0u,
                              GpuVendor::NVIDIA,
                              GpuVendor::Intel,
                              GpuVendor::AMD,
                              GpuVendor::Apple,
                              GpuVendor::Broadcom,
                              GpuVendor::ARM,
                              GpuVendor::Qualcomm,
                              GpuVendor::Samsung,
                              0xDEADu};
  const AVCodecID codecs[]
      = {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_AV1, AV_CODEC_ID_VP9,
         AV_CODEC_ID_PRORES, AV_CODEC_ID_RAWVIDEO, AV_CODEC_ID_NONE};

  for(int api = 0; api <= 5; api++)
  {
    for(uint32_t vendor : vendors)
    {
      for(AVCodecID codec : codecs)
      {
        INFO("api " << api << " vendor " << vendor << " codec "
                    << avcodec_get_name(codec));
        const auto rungs = selectHardwareAccelerations(api, codec, vendor);

        std::set<AVPixelFormat> seen;
        for(AVPixelFormat f : rungs)
        {
          // No rung may repeat: the caller tries them in order and a duplicate
          // is a wasted device-open round trip on every failure.
          CHECK(seen.insert(f).second);
          // Every returned rung has passed both gates by construction; if that
          // ever stops holding the caller opens a device ffmpeg cannot use.
          CHECK(formatIsHardwareDecoded(f));
          CHECK(hardwareDecoderIsAvailable(f));
          CHECK(codecSupportsHWPixelFormat(codec, f, vendor));
        }

        // The singular form is the head of the plural one, nothing else.
        const auto one = selectHardwareAcceleration(api, codec, vendor);
        if(rungs.empty())
          CHECK(one == AV_PIX_FMT_NONE);
        else
          CHECK(one == rungs.front());
      }
    }
  }
}

TEST_CASE("selectHardwareAccelerations honours the vendor ordering",
          "[video][gpuformats]")
{
  // The ordering is the whole point of the per-vendor branches: on Vulkan an
  // NVIDIA GPU must be offered VULKAN before CUDA, an Intel/AMD one VAAPI
  // before VULKAN. It is only observable where both rungs are actually
  // available, so the check is conditional on that -- but it is not vacuous
  // anywhere both exist.
  auto orderOn = [](int api, uint32_t vendor, AVPixelFormat a, AVPixelFormat b) {
    const auto r = selectHardwareAccelerations(api, AV_CODEC_ID_H264, vendor);
    auto ia = std::find(r.begin(), r.end(), a);
    auto ib = std::find(r.begin(), r.end(), b);
    if(ia == r.end() || ib == r.end())
      return true; // one of the two rungs is unavailable here
    return ia < ib;
  };

  CHECK(orderOn(2, GpuVendor::NVIDIA, AV_PIX_FMT_VULKAN, AV_PIX_FMT_CUDA));
  CHECK(orderOn(2, GpuVendor::Intel, AV_PIX_FMT_VAAPI, AV_PIX_FMT_VULKAN));
  CHECK(orderOn(2, GpuVendor::AMD, AV_PIX_FMT_VAAPI, AV_PIX_FMT_VULKAN));
  CHECK(orderOn(2, GpuVendor::ARM, AV_PIX_FMT_DRM_PRIME, AV_PIX_FMT_VULKAN));
  CHECK(orderOn(1, GpuVendor::NVIDIA, AV_PIX_FMT_VAAPI, AV_PIX_FMT_CUDA));

  // An unknown vendor must still get a full generic ladder rather than nothing:
  // whatever the NVIDIA branch produces on this host, the generic branch must
  // not be a strict superset gate that drops everything.
  const auto generic = selectHardwareAccelerations(2, AV_CODEC_ID_H264, 0u);
  for(AVPixelFormat f : generic)
  {
    INFO("generic rung " << name(f));
    CHECK(hardwareDecoderIsAvailable(f));
  }
}

// The two tables that must not be able to disagree.
//
// formatNeedsDecoding(f) == false means "hand this frame to the GPU as it is".
// The GPU end of that promise is score::gfx::createGPUVideoDecoder(), and a
// format for which it returns nullptr renders nothing: VideoNodeRenderer falls
// back to EmptyDecoder and the video is a hole. Neither table can notice that
// on its own -- each is a self-consistent switch over AVPixelFormat.
TEST_CASE("every format that skips swscale has a GPU decoder",
          "[video][gpuformats]")
{
  int checked = 0;
  for(AVPixelFormat f : allPixelFormats())
  {
    if(formatNeedsDecoding(f))
      continue;
    if(formatIsHardwareDecoded(f))
      continue; // the HW transfer decoders, not this factory
    if(isKnownUncoveredByFactory(f))
      continue; // owned by the [!shouldfail] case below

    INFO("format " << name(f) << " skips swscale but has no GPU decoder");
    CHECK(hasGpuDecoder(f));
    checked++;
  }
  // Negative control on the sweep itself: if the enumeration or the filters
  // ever collapse, the loop above silently checks nothing.
  CHECK(checked > 40);
}

// FINDING (reported, not fixed here -- tests-only branch): the big-endian
// grayscale float formats are listed in formatNeedsDecoding()'s "no rescale
// needed" set, but createGPUVideoDecoder() only handles AV_PIX_FMT_GRAYF32 /
// AV_PIX_FMT_GRAYF16, which are the LITTLE-endian aliases on a little-endian
// host. A grayf32be / grayf16be stream therefore skips the CPU rescale AND
// gets no GPU decoder: it renders as nothing at all, where removing it from
// formatNeedsDecoding()'s list would at least have rescaled it to RGBA.
//
// Asserted as the invariant that SHOULD hold, so this flips red -- and must be
// deleted -- the day either table is corrected.
TEST_CASE("FINDING: big-endian grayscale floats skip swscale with no decoder",
          "[video][gpuformats][!shouldfail]")
{
  CHECK_FALSE(formatNeedsDecoding(AV_PIX_FMT_GRAYF32BE));
  CHECK(hasGpuDecoder(AV_PIX_FMT_GRAYF32BE));
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(60, 8, 100)
  CHECK_FALSE(formatNeedsDecoding(AV_PIX_FMT_GRAYF16BE));
  CHECK(hasGpuDecoder(AV_PIX_FMT_GRAYF16BE));
#endif
}

// The converse direction of the same cross-table check: a format the GPU
// factory builds a decoder for must not be sent through sws_scale to RGBA
// first, or the dedicated decoder never runs and the 10/12/16-bit precision it
// exists for is quantised away on the CPU. Swept over every AVPixelFormat this
// ffmpeg knows rather than over a list, so an entry added to either table on
// one side only is caught whichever side it lands on.
TEST_CASE("no format with a GPU decoder is pre-empted by swscale",
          "[video][gpuformats]")
{
  int checked = 0;
  for(AVPixelFormat f : allPixelFormats())
  {
    if(!hasGpuDecoder(f))
      continue;
    INFO("format " << name(f) << " has a GPU decoder but is rescaled first");
    CHECK_FALSE(formatNeedsDecoding(f));
    checked++;
  }
  CHECK(checked > 40);
}

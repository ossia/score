#include <Gfx/Graph/HWAccelSetup.hpp>

#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN
#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace score::gfx
{

std::string hwCodecName(const char* codec_name, AVHWDeviceType device)
{
#if SCORE_HAS_LIBAV && LIBAVUTIL_VERSION_MAJOR >= 57
  return ::Video::hwCodecName(codec_name, device);
#else
  return {};
#endif
}

bool codecSupportsHWPixelFormat(AVCodecID codec_id, AVPixelFormat pix_fmt)
{
#if SCORE_HAS_LIBAV && LIBAVUTIL_VERSION_MAJOR >= 57
  return ::Video::codecSupportsHWPixelFormat(codec_id, pix_fmt);
#else
  return false;
#endif
}

AVPixelFormat selectHardwareAcceleration(
    score::gfx::GraphicsApi api, AVCodecID codec_id, QRhi* rhi)
{
  uint32_t vendorId = 0;
#if QT_HAS_VULKAN
  if(rhi && rhi->backend() == QRhi::Vulkan)
  {
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(
        rhi->nativeHandles());
    if(nh && nh->physDev)
    {
      QVulkanInstance* inst = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      inst = nh->inst;
#else
      inst = staticVulkanInstance(false);
#endif
      if(inst)
      {
        VkPhysicalDeviceProperties props{};
        inst->functions()->vkGetPhysicalDeviceProperties(
            nh->physDev, &props);
        vendorId = props.vendorID;
      }
    }
  }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if(!vendorId && rhi)
    vendorId = rhi->driverInfo().vendorId;
#endif

#if SCORE_HAS_LIBAV && LIBAVUTIL_VERSION_MAJOR >= 57
  return ::Video::selectHardwareAcceleration(
      static_cast<int>(api), codec_id, vendorId);
#else
  return AV_PIX_FMT_NONE;
#endif
}

} // namespace score::gfx

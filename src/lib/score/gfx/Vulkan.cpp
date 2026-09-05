#include <score/gfx/Vulkan.hpp>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#include <QVulkanInstance>

#if __has_include(<QtGui/private/qrhi_p.h>)
#include <QtGui/private/qrhi_p.h>
#endif

#if __has_include(<rhi/qrhi_platform.h>)
#include <rhi/qrhi_platform.h>
#elif __has_include(<private/qrhivulkan_p.h>)
#include <private/qrhivulkan_p.h>
#endif

#include <mutex>
namespace score::gfx
{
static QVulkanInstance* g_staticVulkanInstance{};
static std::once_flag g_staticVulkanInstanceInit{};
static bool g_staticVulkanInstanceInvalid = false;

QVulkanInstance* staticVulkanInstance(bool create)
{
  if(g_staticVulkanInstanceInvalid)
    return nullptr;

  if(!create)
    return g_staticVulkanInstance;

  std::call_once(g_staticVulkanInstanceInit, [=]() {
    g_staticVulkanInstance = new QVulkanInstance{};
    QVulkanInstance& instance = *g_staticVulkanInstance;

    const int validationLevel
        = qEnvironmentVariableIntValue("SCORE_GPU_VALIDATION");
#if !defined(NDEBUG)
    const bool enableValidation = true;
#else
    const bool enableValidation = validationLevel > 0;
#endif
    if(enableValidation)
    {
      instance.setLayers({"VK_LAYER_KHRONOS_validation"});

      // Level 2 is the deliberately expensive validation soak. The standard
      // layer only enables core/object-lifetime checks by default; these
      // switches add the GPU-timeline, synchronization and best-practice
      // checks which find descriptor and resource-state bugs core validation
      // cannot see. Respect an explicit caller setting.
      if(validationLevel > 1)
      {
        const auto setDefault = [](const char* name) {
          if(!qEnvironmentVariableIsSet(name))
            qputenv(name, "1");
        };
        setDefault("VK_LAYER_VALIDATE_SYNC");
        setDefault("VK_LAYER_GPUAV_ENABLE");
        setDefault("VK_LAYER_GPUAV_SAFE_MODE");
        setDefault("VK_LAYER_VALIDATE_BEST_PRACTICES");
        setDefault("VK_LAYER_THREAD_SAFETY");
      }
    }

    QByteArrayList exts;
    exts << QRhiVulkanInitParams::preferredInstanceExtensions();

    if(auto v = instance.supportedApiVersion(); v >= QVersionNumber(1, 1))
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
      // Without qtbase@3bfc5d0b3b979a8249ca1cfc38e2d3052a3c7c6f
      // we may hit vmaMemoryAllocator asserts if asking for vk 1.4
      if(v >= QVersionNumber(1, 4))
        v = QVersionNumber(1, 3);
#endif
      instance.setApiVersion(v);
    }
    else
    {
      exts << "VK_KHR_maintenance1";
    }

    instance.setExtensions(exts);
    // QVulkanInstance's redirect is the only consumer of validation messages
    // in score. Suppressing it while the layer is active makes a validation
    // run look clean regardless of what the layer reports.
    if(!enableValidation)
      instance.setFlags(QVulkanInstance::Flag::NoDebugOutputRedirect);

    if(!instance.create())
    {
      // The instance is deleted immediately below, so this is the only place
      // errorCode() and the supported layer/extension lists can still be read.
      qWarning() << "Vulkan: QVulkanInstance::create() failed, VkResult ="
                 << instance.errorCode();

      // Nothing supported at all is a different fault from a missing layer: it
      // means the QPA plugin has no Vulkan support (the offscreen platform is
      // the usual one, and it says "does not support createPlatformVulkanInstance"
      // just above).
      if(instance.supportedLayers().isEmpty()
         && instance.supportedExtensions().isEmpty())
      {
        qWarning() << "Vulkan: the platform plugin provides no Vulkan support "
                      "-- this is expected under QT_QPA_PLATFORM=offscreen";
        g_staticVulkanInstanceInvalid = true;
        delete g_staticVulkanInstance;
        g_staticVulkanInstance = nullptr;
        return;
      }

      qWarning() << "Vulkan: requested api" << instance.apiVersion()
                 << "supported" << instance.supportedApiVersion();

      const auto missing = [](const auto& wanted, const auto& supported) {
        QByteArrayList out;
        for(const auto& w : wanted)
          if(!supported.contains(w))
            out << w;
        return out;
      };
      if(const auto m = missing(instance.layers(), instance.supportedLayers());
         !m.isEmpty())
        qWarning() << "Vulkan: requested layers that are NOT available:" << m;
      if(const auto m
         = missing(instance.extensions(), instance.supportedExtensions());
         !m.isEmpty())
        qWarning() << "Vulkan: requested extensions that are NOT available:" << m;

      g_staticVulkanInstanceInvalid = true;
      delete g_staticVulkanInstance;
      g_staticVulkanInstance = nullptr;
    }
  });

  // Re-check: on the very first call, create() may just have failed inside
  // call_once — returning the half-initialized instance would send callers
  // (Graph's API-fallback check, QRhi::create) straight into a crash.
  if(g_staticVulkanInstanceInvalid)
    return nullptr;

  return g_staticVulkanInstance;
}
}
#endif

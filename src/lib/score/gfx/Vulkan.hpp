#pragma once
#include <QtGui/qtguiglobal.h>

#include <score_lib_base_export.h>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) \
&& __has_include(<vulkan/vulkan.h>) && !defined(_MSC_VER) && __has_include(<QtGui/private/qrhivulkan_p.h>) && __has_include(<QVulkanInstance>)
#define QT_HAS_VULKAN 1
struct QRhiVulkanNativeHandles;

class QVulkanInstance;
namespace score::gfx
{
SCORE_LIB_BASE_EXPORT
QVulkanInstance* staticVulkanInstance(bool create = true);
SCORE_LIB_BASE_EXPORT
QRhiVulkanNativeHandles* createRayTracingRhi(QVulkanInstance* vulkanInstance);
}
#else
#define QT_HAS_VULKAN 0
#endif

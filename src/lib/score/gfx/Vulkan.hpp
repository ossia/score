#pragma once
#include <QtGui/qtguiglobal.h>

#include <score_lib_base_export.h>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan) \
    && __has_include(<vulkan/vulkan.h>) && !defined(_MSC_VER)
#define QT_HAS_VULKAN 1

class QVulkanInstance;
namespace score::gfx
{
SCORE_LIB_BASE_EXPORT
QVulkanInstance* staticVulkanInstance();
}
#else
#define QT_HAS_VULKAN 0
#endif

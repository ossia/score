#pragma once
#include <QtGui/qtguiglobal.h>
#include <score_lib_base_export.h>

#if defined(QT_FEATURE_vulkan) && QT_CONFIG(vulkan)
#define QT_HAS_VULKAN 1

class QVulkanInstance;
namespace score::gfx
{
SCORE_LIB_BASE_EXPORT
QVulkanInstance* staticVulkanInstance();
}
#endif

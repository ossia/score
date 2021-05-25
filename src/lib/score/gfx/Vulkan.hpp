#pragma once
#include <QtGui/qtguiglobal.h>
#include <score_lib_base_export.h>

#if QT_CONFIG(vulkan)
class QVulkanInstance;
namespace score::gfx
{
SCORE_LIB_BASE_EXPORT
QVulkanInstance* staticVulkanInstance();
}
#endif

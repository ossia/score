#pragma once
#include <qnamespace.h>
#include <iscore_lib_base_export.h>
class QLayout;

namespace iscore
{
/**
 * @brief clearLayout Recursively remove all the items & widgets of a layout.
 * @param layout Layout to clear
 */
ISCORE_LIB_BASE_EXPORT void clearLayout(QLayout* layout);

/**
 * @brief setCursor sets the cursor safely.
 */
ISCORE_LIB_BASE_EXPORT void setCursor(Qt::CursorShape c);
}

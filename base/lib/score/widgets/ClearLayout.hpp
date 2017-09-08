#pragma once
#include <score_lib_base_export.h>
#include <qnamespace.h>
class QLayout;

namespace score
{
/**
 * @brief clearLayout Recursively remove all the items & widgets of a layout.
 * @param layout Layout to clear
 */
SCORE_LIB_BASE_EXPORT void clearLayout(QLayout* layout);

/**
 * @brief setCursor sets the cursor safely.
 */
SCORE_LIB_BASE_EXPORT void setCursor(Qt::CursorShape c);
}

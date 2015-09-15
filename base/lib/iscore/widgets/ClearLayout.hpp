#pragma once
#include <qnamespace.h>
class QLayout;

namespace iscore
{
/**
 * @brief clearLayout Recursively remove all the items & widgets of a layout.
 * @param layout Layout to clear
 */
void clearLayout(QLayout* layout);

/**
 * @brief setCursor sets the cursor safely.
 */
void setCursor(Qt::CursorShape c);
}

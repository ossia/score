#pragma once
#include <score_lib_base_export.h>

class QBrush;
class QPalette;

namespace score
{
/**
 * @brief The fill of a bang: the circle an impulse gets.
 *
 * Shared by score::QGraphicsButton in the node view and the device explorer's
 * tree rows and address panel.
 *
 * Falls back to the palette when the skin has no brushes to give -- a no-GUI
 * application context leaves them default-constructed.
 *
 * @param lit The pressed state.
 */
SCORE_LIB_BASE_EXPORT QBrush bangFill(const QPalette& fallback, bool lit);
}

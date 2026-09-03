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
 * Falls back to the palette when the skin has not been loaded: its brushes are
 * default-constructed until the application installs the skin resource, and a
 * bang in transparent black is invisible.
 *
 * @param lit The pressed state.
 */
SCORE_LIB_BASE_EXPORT QBrush bangFill(const QPalette& fallback, bool lit);
}

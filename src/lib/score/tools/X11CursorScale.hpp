#pragma once
#include <score_lib_base_export.h>

namespace score
{
/**
 * On X11 the pointer does not follow the scale factor of an application: theme
 * cursors come at the size of the desktop, and pixmap cursors are drawn pixel
 * for pixel. score draws all of its cursors itself, at the desktop's pointer
 * size times the zoom (see CursorTheme). The theme cursors are asked at that
 * size too, for what score does not draw: the instant between Qt setting a
 * standard shape and score replacing it, and the editors of plug-ins, which
 * load theme cursors in score's process. Child processes inherit it with
 * QT_SCALE_FACTOR.
 *
 * Must run before the QGuiApplication is created: the size of the theme
 * cursors is read once, when the platform plug-in starts.
 */
SCORE_LIB_BASE_EXPORT void setupX11CursorScale(double zoom);

//! The desktop's pointer size over the 24 pixels score's cursor artwork is
//! drawn next to, without the zoom: the scale of the screen for cursors on
//! X11. 0 when there is no X11 display.
SCORE_LIB_BASE_EXPORT double x11DesktopCursorScale() noexcept;
}

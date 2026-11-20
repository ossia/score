#pragma once
#include <score_lib_base_export.h>

namespace score
{
/**
 * On X11 the pointer does not follow the scale factor of an application: theme
 * cursors come at the size of the desktop, and pixmap cursors are drawn pixel
 * for pixel. This makes both follow the zoom of the application: the theme
 * cursors are asked at the desktop's size times the zoom, and
 * x11CursorScale() says by how much to scale the artwork of pixmap cursors.
 *
 * Must run before the QGuiApplication is created: the size of the theme
 * cursors is read once, when the platform plug-in starts.
 */
SCORE_LIB_BASE_EXPORT void setupX11CursorScale(double zoom);

//! Size of a pixmap cursor relative to its 1x artwork, drawn for a 24 pixel
//! pointer. 0 when setupX11CursorScale() did not apply (other platforms).
SCORE_LIB_BASE_EXPORT double x11CursorScale() noexcept;
}

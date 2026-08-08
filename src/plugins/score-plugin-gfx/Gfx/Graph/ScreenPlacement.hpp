#pragma once
#include <QList>
#include <QSet>

#include <score_plugin_gfx_export.h>

class QScreen;

namespace score::gfx
{
/**
 * @brief Whether a screen here can back more than one window.
 *
 * On a desktop a window manager gives every window a surface of its own. With
 * no windowing system at all -- eglfs, vkkhrdisplay, linuxfb, which is what
 * runs on an embedded board with nothing but a framebuffer -- a screen *is*
 * the scanout buffer, and Qt gives it to whichever window asks first.
 *
 * The second one does not fail politely. QEglFSWindow::create() calls qFatal
 * ("EGLFS: OpenGL windows cannot be mixed with others.") and the process is
 * gone, taking the score with it. So this is a question that has to be asked
 * before opening a window, not a failure to recover from after.
 */
SCORE_PLUGIN_GFX_EXPORT bool oneWindowPerScreen() noexcept;

//! The screens already carrying a window of this process.
SCORE_PLUGIN_GFX_EXPORT QSet<QScreen*> occupiedScreens();

/**
 * @brief A screen a new window may be given, or nullptr when they are all taken.
 *
 * `preferred` -- the screen the user picked -- wins whenever it is free.
 */
SCORE_PLUGIN_GFX_EXPORT QScreen* freeScreen(
    QScreen* preferred, const QList<QScreen*>& all,
    const QSet<QScreen*>& taken) noexcept;
}

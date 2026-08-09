#pragma once
#include <score/gfx/DisplayConfig.hpp>

#include <QList>
#include <QSet>

#include <score_plugin_gfx_export.h>

class QScreen;

namespace score::gfx
{

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

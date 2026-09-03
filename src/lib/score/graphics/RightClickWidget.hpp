#pragma once
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPointer>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief The one type-in box a right-click may have open.
 *
 * Right-clicking a second control -- or the same one twice -- used to leave
 * the first box floating over the scene with nobody owning it: it is parented
 * to the scene, not to the control, so nothing took it down.
 *
 * A QPointer, so that a box torn down by its own control leaves nothing
 * dangling here.
 */
SCORE_LIB_BASE_EXPORT QPointer<QGraphicsProxyWidget>& currentRightClickWidget();

//! Takes down whatever right-click box is open, if any.
inline void closeRightClickWidget()
{
  if(auto* cur = currentRightClickWidget().data())
  {
    if(auto* sc = cur->scene())
      sc->removeItem(cur);
    delete cur;
  }
}
}

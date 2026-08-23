#pragma once
#include <QPoint>

#include <score_lib_process_export.h>

namespace Dataflow
{
/**
 * @brief A QGraphicsView that knows how to move its content for auto-scroll.
 *
 * Implemented (by multiple inheritance, next to QGraphicsView) by views whose
 * scrolling is not plain scroll bars: the nodal canvas pans an item, the
 * timeline grows its scene as one scrolls past the end. CableDragAutoScroller
 * looks it up with dynamic_cast and falls back to the scroll bars otherwise.
 */
class SCORE_LIB_PROCESS_EXPORT AutoScrollableView
{
public:
  virtual ~AutoScrollableView();

  /**
   * @brief Reveals what lies `delta` pixels further (view coordinates).
   *
   * A positive x reveals content to the right, a positive y content below.
   * @return true if the content moved.
   */
  virtual bool autoScrollBy(QPoint delta) = 0;
};
}

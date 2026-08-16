#pragma once
#include <QPointF>
#include <QRectF>

#include <score_lib_base_export.h>

class QGraphicsItem;
class QGraphicsSceneMouseEvent;

namespace score
{

struct SCORE_LIB_BASE_EXPORT InfiniteScroller
{
  static QRectF currentGeometry;
  static double origValue;
  static double currentSpeed;
  static double currentDelta;

  static void start(QGraphicsItem& self, double orig);
  static void move_free(QGraphicsSceneMouseEvent* event);
  static double move(QGraphicsSceneMouseEvent* event);
  static void stop(QGraphicsItem& self, QGraphicsSceneMouseEvent* event);

  //! stop() for a drag that ends without a release event to end it on: undoes
  //! what start() did to the cursor, but cannot put it back where the press
  //! was, there being no press position to read. A no-op for an item that is
  //! not the one currently dragging, so a control cleaning up after itself
  //! cannot tear down someone else's session.
  static void abort(QGraphicsItem& self);

  //! Motion for a drag that is not a vertical value scroll: the platform's
  //! relative delta while it holds the pointer, the change in pointer position
  //! otherwise. Both axes, since the caller decides what they mean.
  static QPointF relativeMotion(QGraphicsSceneMouseEvent* event);

  //! Whether the platform is holding the pointer on the press point, so that a
  //! caller keeping its own cursor knows whether hiding it is safe.
  static bool holdsPointer();

  //! Ends any relative-motion session in progress, without touching the cursor.
  static void cancel();
};
}

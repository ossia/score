#pragma once
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

  //! Ends any relative-motion session in progress, without touching the cursor.
  static void cancel();
};
}

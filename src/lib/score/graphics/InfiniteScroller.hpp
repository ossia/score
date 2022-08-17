#pragma once
#include <score/tools/Cursor.hpp>

#include <QApplication>
#include <QGraphicsItem>
#include <QScreen>

namespace score
{

struct InfiniteScroller
{
  static inline QRectF currentGeometry{};
  static inline double origValue{};
  static inline double currentDelta{};

  static void start(QGraphicsItem& self, double orig)
  {
    currentDelta = 0.;
    origValue = orig;

#if !defined(__EMSCRIPTEN__)
    self.setCursor(QCursor(Qt::BlankCursor));
#endif
    currentGeometry = qApp->primaryScreen()->availableGeometry();
  }

  static double move(QGraphicsSceneMouseEvent* event)
  {
    auto delta = (event->screenPos().y() - event->lastScreenPos().y());
    double ratio = qApp->keyboardModifiers() & Qt::CTRL ? 0.2 : 1.;
    if(std::abs(delta) < 500)
    {
      currentDelta += ratio * delta;
    }

    const double max = currentGeometry.height();

    if(event->screenPos().y() <= 100)
    {
      score::setCursorPos(QPointF(event->screenPos().x(), max - 101));
    }
    else if(event->screenPos().y() >= (max - 100))
    {
      score::setCursorPos(QPointF(event->screenPos().x(), 101));
    }

    double v = origValue - currentDelta / max;
    if(v <= 0.)
    {
      currentDelta = origValue * max;
      v = 0.;
    }
    else if(v >= 1.)
    {
      currentDelta = ((origValue - 1.) * max);
      v = 1.;
    }

    return v;
  }

  static void stop(QGraphicsItem& self, QGraphicsSceneMouseEvent* event)
  {
    score::setCursorPos(event->buttonDownScreenPos(Qt::LeftButton));
    self.unsetCursor();
  }
};
}

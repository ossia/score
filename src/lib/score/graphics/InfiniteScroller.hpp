#pragma once
#include <score/tools/Cursor.hpp>

#include <QGraphicsItem>
#include <QGuiApplication>
#include <QScreen>

namespace score
{

struct InfiniteScroller
{
  static inline QRectF currentGeometry{};
  static inline double origValue{};
  static inline double currentSpeed{};
  static inline double currentDelta{};

  static void start(QGraphicsItem& self, double orig)
  {
    currentDelta = 0.;
    origValue = orig;

#if !defined(__EMSCRIPTEN__)
    self.setCursor(QCursor(Qt::BlankCursor));
#endif
    auto* screen = qGuiApp->screenAt(QCursor::pos());
    if(!screen)
      screen = qGuiApp->primaryScreen();
    currentGeometry = screen->availableGeometry();
  }

  static void move_free(QGraphicsSceneMouseEvent* event)
  {
    auto delta = (event->screenPos().y() - event->lastScreenPos().y());
    double ratio = qGuiApp->keyboardModifiers() & Qt::CTRL ? 0.2 : 1.;
    if(std::abs(delta) < 500)
    {
      currentSpeed = ratio * delta;
      currentDelta += ratio * delta;
    }

    const double top = currentGeometry.top();
    const double bottom = currentGeometry.bottom();
    if(event->screenPos().y() <= top + 100)
    {
      score::setCursorPos(QPointF(event->screenPos().x(), bottom - 101));
    }
    else if(event->screenPos().y() >= bottom - 100)
    {
      score::setCursorPos(QPointF(event->screenPos().x(), top + 101));
    }
  }

  static double move(QGraphicsSceneMouseEvent* event)
  {
    move_free(event);

    const double max = currentGeometry.height();
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

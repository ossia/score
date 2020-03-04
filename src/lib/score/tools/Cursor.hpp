#pragma once
// See https://github.com/LMMS/lmms/issues/5194 for the rationale.
#include <QPointF>
#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#else
#include <QCursor>
#endif
namespace score
{
inline void setCursorPos(QPointF pos) noexcept
{
#if defined(__APPLE__)
  CGPoint ppos;
  ppos.x = pos.x();
  ppos.y = pos.y();

  CGEventRef e = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, ppos, kCGMouseButtonLeft);
  CGEventPost(kCGHIDEventTap, e);
  CFRelease(e);
#else
  QCursor::setPos(pos);
#endif
}
inline void moveCursorPos(QPointF pos) noexcept
{
#if defined(__APPLE__)
  static int i = 0;
  i++;
  if (i%2) {
    // Moving a cursor is visibly an expensive operation on macos
    // even on 3.2ghz i7 CPUs so we cull it a bit
    return;
  }
  CGPoint ppos;
  ppos.x = pos.x();
  ppos.y = pos.y();

  CGEventRef e = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, ppos, kCGMouseButtonLeft);
  CGEventPost(kCGHIDEventTap, e);
  CFRelease(e);
#else
  QCursor::setPos(pos);
#endif
}
}

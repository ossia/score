#pragma once
#include <QPointF>

#include <score_lib_base_export.h>

class QWindow;

namespace score
{
//! Relative ("locked") mouse motion, when the platform provides it.
struct SCORE_LIB_BASE_EXPORT PointerLock
{
  using MotionCallback = void (*)(QPointF);
  using ReleaseCallback = void (*)();

  static bool beginRelative(
      QWindow* window, MotionCallback onMotion, ReleaseCallback onRelease) noexcept;
  static bool active() noexcept;
  static QPointF takeDelta() noexcept;
  static void endRelative() noexcept;
};
}

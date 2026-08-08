#pragma once
#include <score/tools/PointerLock.hpp>

class QWindow;

namespace score::x11
{
//! XInput2 + pointer-confine backend, used when the xcb plugin is in charge.
bool begin(
    QWindow* window, PointerLock::MotionCallback onMotion,
    PointerLock::ReleaseCallback onRelease) noexcept;
bool active() noexcept;
QPointF takeDelta() noexcept;
void end() noexcept;
}

#include <score/tools/PointerLock.hpp>

#include <QCoreApplication>
#include <QTimer>

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>

namespace score
{
namespace
{
::id g_monitor{};
QTimer* g_watchdog{};
PointerLock::MotionCallback g_callback{};
PointerLock::ReleaseCallback g_release{};
double g_dx{};
double g_dy{};
bool g_active{};

struct Reassociate
{
  ~Reassociate()
  {
    if(g_active)
      CGAssociateMouseAndMouseCursorPosition(true);
  }
};

const Reassociate g_reassociate;
}

bool PointerLock::beginRelative(
    QWindow*, MotionCallback onMotion, ReleaseCallback onRelease) noexcept
{
  if(g_active)
    return true;

  if(CGAssociateMouseAndMouseCursorPosition(false) != kCGErrorSuccess)
    return false;

  g_dx = 0.;
  g_dy = 0.;
  g_callback = onMotion;
  g_release = onRelease;
  g_active = true;

  g_monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:(NSEventMaskMouseMoved
                                            | NSEventMaskLeftMouseDragged
                                            | NSEventMaskRightMouseDragged
                                            | NSEventMaskOtherMouseDragged)
                                   handler:^NSEvent*(NSEvent* ev) {
                                     const QPointF delta{[ev deltaX], [ev deltaY]};
                                     g_dx += delta.x();
                                     g_dy += delta.y();
                                     if(g_callback)
                                       g_callback(delta);
                                     return ev;
                                   }];

  if(!g_watchdog)
  {
    g_watchdog = new QTimer{qApp};
    g_watchdog->setInterval(500);
    QObject::connect(g_watchdog, &QTimer::timeout, qApp, [] {
      if([NSEvent pressedMouseButtons] == 0)
      {
        if(g_release)
          g_release();
        PointerLock::endRelative();
      }
    });
    QObject::connect(
        qApp, &QCoreApplication::aboutToQuit, qApp, [] { PointerLock::endRelative(); });
  }
  g_watchdog->start();

  return true;
}

bool PointerLock::active() noexcept
{
  return g_active;
}

QPointF PointerLock::takeDelta() noexcept
{
  const QPointF d{g_dx, g_dy};
  g_dx = 0.;
  g_dy = 0.;
  return d;
}

void PointerLock::endRelative() noexcept
{
  if(!g_active)
    return;

  if(g_watchdog)
    g_watchdog->stop();
  if(g_monitor)
  {
    [NSEvent removeMonitor:g_monitor];
    g_monitor = nil;
  }

  CGAssociateMouseAndMouseCursorPosition(true);
  g_active = false;
  g_callback = nullptr;
  g_release = nullptr;
  g_dx = 0.;
  g_dy = 0.;
}
}

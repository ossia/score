#include "IdleInhibitor.hpp"

#include <QCoreApplication>
#include <QDebug>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__) && TARGET_OS_OSX
#include <IOKit/pwr_mgt/IOPMLib.h>
#elif defined(SCORE_GFX_HAS_DBUS)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#endif

namespace score::gfx
{
namespace
{
int g_count = 0;

#if defined(_WIN32)
void acquire()
{
  SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
}
void release()
{
  SetThreadExecutionState(ES_CONTINUOUS);
}
#elif defined(__APPLE__) && TARGET_OS_OSX
IOPMAssertionID g_assertion = kIOPMNullAssertionID;
void acquire()
{
  IOPMAssertionCreateWithName(
      kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
      CFSTR("ossia score output window"), &g_assertion);
}
void release()
{
  if(g_assertion != kIOPMNullAssertionID)
    IOPMAssertionRelease(g_assertion);
  g_assertion = kIOPMNullAssertionID;
}
#elif defined(SCORE_GFX_HAS_DBUS)
uint32_t g_cookie = 0;
bool g_hasCookie = false;
void acquire()
{
  auto bus = QDBusConnection::sessionBus();
  if(!bus.isConnected())
    return;
  auto msg = QDBusMessage::createMethodCall(
      "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", "Inhibit");
  msg << QCoreApplication::applicationName() << QStringLiteral("Output window");
  QDBusReply<uint32_t> reply = bus.call(msg, QDBus::Block, 2000);
  if(reply.isValid())
  {
    g_cookie = reply.value();
    g_hasCookie = true;
  }
  else
  {
    qDebug() << "Could not inhibit the screen saver:" << reply.error().message();
  }
}
void release()
{
  if(!g_hasCookie)
    return;
  auto msg = QDBusMessage::createMethodCall(
      "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", "UnInhibit");
  msg << g_cookie;
  QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
  g_hasCookie = false;
}
#else
void acquire() { }
void release() { }
#endif
}

IdleInhibitor::IdleInhibitor()
{
  if(g_count++ == 0)
    acquire();
}

IdleInhibitor::~IdleInhibitor()
{
  if(--g_count == 0)
    release();
}

int IdleInhibitor::activeCount() noexcept
{
  return g_count;
}
}

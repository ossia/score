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
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
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
enum class Inhibit
{
  None,
  Pending,
  Held
};
Inhibit g_state = Inhibit::None;
uint32_t g_cookie = 0;
bool g_wanted = false;

QDBusMessage screenSaverCall(const QString& method)
{
  return QDBusMessage::createMethodCall(
      "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", method);
}

void unInhibit()
{
  auto msg = screenSaverCall(QStringLiteral("UnInhibit"));
  msg << g_cookie;
  QDBusConnection::sessionBus().send(msg);
  g_state = Inhibit::None;
}

void acquire()
{
  g_wanted = true;
  if(g_state != Inhibit::None)
    return;
  auto* app = QCoreApplication::instance();
  auto bus = QDBusConnection::sessionBus();
  if(!app || !bus.isConnected())
    return;
  auto msg = screenSaverCall(QStringLiteral("Inhibit"));
  msg << QCoreApplication::applicationName() << QStringLiteral("Output window");
  g_state = Inhibit::Pending;
  auto* watcher = new QDBusPendingCallWatcher{bus.asyncCall(msg), app};
  QObject::connect(
      watcher, &QDBusPendingCallWatcher::finished, app,
      [](QDBusPendingCallWatcher* w) {
    w->deleteLater();
    const QDBusPendingReply<uint32_t> reply = *w;
    if(reply.isError())
    {
      g_state = Inhibit::None;
      qDebug() << "Could not inhibit the screen saver:" << reply.error().message();
      return;
    }
    g_cookie = reply.value();
    g_state = Inhibit::Held;
    if(!g_wanted)
      unInhibit();
  });
}

void release()
{
  g_wanted = false;
  if(g_state == Inhibit::Held)
    unInhibit();
}
#else
void acquire() { }
void release() { }
#endif
}

IdleInhibitor::~IdleInhibitor()
{
  release();
}

void IdleInhibitor::acquire()
{
  if(m_held)
    return;
  m_held = true;
  if(g_count++ == 0)
    score::gfx::acquire();
}

void IdleInhibitor::release()
{
  if(!m_held)
    return;
  m_held = false;
  if(--g_count == 0)
    score::gfx::release();
}
}

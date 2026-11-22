#include "IdleInhibitor.hpp"

#include <score/tools/Debug.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QThread>

#include <utility>

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
ScreenSaverInhibition g_inhibition;

QDBusMessage screenSaverCall(const QString& method)
{
  return QDBusMessage::createMethodCall(
      "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", method);
}

void unInhibit(uint32_t cookie)
{
  if(!QCoreApplication::instance())
    return;
  auto msg = screenSaverCall(QStringLiteral("UnInhibit"));
  msg << cookie;
  QDBusConnection::sessionBus().send(msg);
}

void acquire()
{
  if(!g_inhibition.want())
    return;
  auto* app = QCoreApplication::instance();
  auto bus = QDBusConnection::sessionBus();
  if(!app || !bus.isConnected())
  {
    g_inhibition.failed();
    return;
  }
  auto msg = screenSaverCall(QStringLiteral("Inhibit"));
  msg << QCoreApplication::applicationName() << QStringLiteral("Output window");
  auto* watcher = new QDBusPendingCallWatcher{bus.asyncCall(msg), app};
  QObject::connect(
      watcher, &QDBusPendingCallWatcher::finished, app,
      [](QDBusPendingCallWatcher* w) {
    w->deleteLater();
    const QDBusPendingReply<uint32_t> reply = *w;
    if(reply.isError())
    {
      g_inhibition.failed();
      static bool warned = false;
      if(!std::exchange(warned, true))
        qDebug() << "Could not inhibit the screen saver:" << reply.error().message();
      return;
    }
    if(g_inhibition.replied(reply.value()))
      unInhibit(g_inhibition.cookie());
  });
}

void release()
{
  if(g_inhibition.unwant())
    unInhibit(g_inhibition.cookie());
}
#else
void acquire() { }
void release() { }
#endif

bool reachesOS()
{
  static const bool on = [] {
    if(qEnvironmentVariableIsSet("SCORE_NO_IDLE_INHIBIT"))
      return false;
    const QString platform = QGuiApplication::platformName();
    return platform != QLatin1String("offscreen") && platform != QLatin1String("minimal");
  }();
  return on;
}

bool onGuiThread()
{
  auto* app = QCoreApplication::instance();
  return !app || QThread::currentThread() == app->thread();
}
}

int IdleInhibitor::holders() noexcept
{
  return g_count;
}

IdleInhibitor::~IdleInhibitor()
{
  release();
}

void IdleInhibitor::acquire()
{
  if(m_held)
    return;
  SCORE_ASSERT(onGuiThread());
  m_held = true;
  if(g_count++ == 0 && reachesOS())
    score::gfx::acquire();
}

void IdleInhibitor::release()
{
  if(!m_held)
    return;
  SCORE_ASSERT(onGuiThread());
  m_held = false;
  if(--g_count == 0 && reachesOS())
    score::gfx::release();
}
}

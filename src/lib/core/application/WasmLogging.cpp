#include "WasmLogging.hpp"

#if defined(__EMSCRIPTEN__)
#include <score/tools/Debug.hpp>

#include <QFileInfo>
#include <QMessageLogContext>
#include <QString>

#include <emscripten/console.h>

#include <mutex>

namespace score::wasm
{
namespace
{
// Deliberately not enabled on desktop: there a flood is survivable, stderr is
// cheap, and dropping lines would change what developers see in a build they
// rely on for debugging.
constexpr qint64 throttle_threshold = 5;
constexpr qint64 throttle_heartbeat = 10000;

std::mutex g_throttleMutex;
QString g_lastMessage;
qint64 g_repeatCount = 0;
thread_local bool t_lastSuppressed = false;

// Like fprintf, these never re-enter Qt's logging, so they are safe to use from
// inside the message handler.
void emitLine(QtMsgType type, const char* line)
{
  switch(type)
  {
    case QtDebugMsg:
    case QtInfoMsg:
      emscripten_console_log(line);
      break;
    case QtWarningMsg:
      emscripten_console_warn(line);
      break;
    case QtCriticalMsg:
    case QtFatalMsg:
      emscripten_console_error(line);
      break;
  }
}

// Returns how many suppressed repeats the caller should report now (0 for
// none), and sets @p suppress when this message must be dropped.
qint64 throttleStep(const QString& msg, bool& suppress)
{
  const std::lock_guard lock{g_throttleMutex};

  if(msg == g_lastMessage)
  {
    ++g_repeatCount;
    suppress = g_repeatCount > throttle_threshold;
    // Still say something once in a while: a permanently repeating message
    // must not look like silence.
    if(suppress && (g_repeatCount % throttle_heartbeat) == 0)
      return g_repeatCount;
    return 0;
  }

  const qint64 pending = g_repeatCount > throttle_threshold ? g_repeatCount : 0;
  g_lastMessage = msg;
  g_repeatCount = 1;
  suppress = false;
  return pending;
}
}

void logMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
  if(type != QtFatalMsg)
  {
    bool suppress = false;
    const qint64 repeats = throttleStep(msg, suppress);

    if(repeats > 0)
    {
      const auto summary
          = QStringLiteral("Info: [previous message repeated %1 times]").arg(repeats);
      emitLine(QtInfoMsg, summary.toUtf8().constData());
    }

    t_lastSuppressed = suppress;
    if(suppress)
      return;
  }
  else
  {
    t_lastSuppressed = false;
  }

  // QtDebugMsg=0, QtWarningMsg=1, QtCriticalMsg=2, QtFatalMsg=3, QtInfoMsg=4
  static const char* const prefixes[]
      = {"Debug", "Warning", "Critical", "Fatal", "Info"};
  const int idx = int(type);
  const char* prefix = (idx >= 0 && idx <= int(QtInfoMsg)) ? prefixes[idx] : "Log";

  const QByteArray line
      = QStringLiteral("%1: %2 (%3:%4)")
            .arg(
                QString::fromUtf8(prefix), msg,
                QFileInfo(context.file).baseName())
            .arg(context.line)
            .toUtf8();
  emitLine(type, line.constData());
}

bool lastMessageWasSuppressed() noexcept
{
  return t_lastSuppressed;
}
}
#endif

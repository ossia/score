// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SafeQApplication.hpp"

#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/std/Invoke.hpp>

#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>
#include <QThread>

#include <wobjectimpl.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/console.h>

#include <mutex>
#endif

W_OBJECT_IMPL(SafeQApplication)

SafeQApplication::~SafeQApplication() { }

#if defined(__EMSCRIPTEN__)
namespace
{
// Repeated-message throttle, wasm only.
//
// On wasm every log line is an fprintf that goes through _fd_write into
// console.*, and the browser captures a stack for each one. A runaway warning
// -- Qt's own "QRhiGles2: Context is lost." once per frame, say, which we
// cannot patch out -- therefore does not merely fill the console: it makes
// DevTools unresponsive, which is exactly when the user needs it to run the
// diagnostics. Collapse consecutive identical messages, syslog style.
//
// Deliberately not enabled on desktop: there a flood is survivable, stderr is
// cheap, and dropping lines would change what developers see in a build they
// rely on for debugging. Flipping that is a matter of widening this #if.
constexpr qint64 log_throttle_threshold = 5;
constexpr qint64 log_throttle_heartbeat = 10000;

std::mutex g_throttleMutex;
QString g_lastMessage;
qint64 g_repeatCount = 0;
thread_local bool t_lastSuppressed = false;

// Emit one already-formatted line in a single call.
//
// stdio is pathologically expensive here: fprintf goes fiprintf -> vfiprintf ->
// __stdio_write -> _fd_write -> doWritev -> write -> put_char, i.e. it crosses
// into JS one character at a time, and the whole line is then re-parsed out of
// the fd. emscripten_console_* hands the string straight to console.* instead.
// Like fprintf, these never re-enter Qt's logging, so they are also safe to use
// from inside the message handler.
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
    suppress = g_repeatCount > log_throttle_threshold;
    // Still say something once in a while: a permanently repeating message
    // must not look like silence.
    if(suppress && (g_repeatCount % log_throttle_heartbeat) == 0)
      return g_repeatCount;
    return 0;
  }

  const qint64 pending = g_repeatCount > log_throttle_threshold ? g_repeatCount : 0;
  g_lastMessage = msg;
  g_repeatCount = 1;
  suppress = false;
  return pending;
}
}

bool SafeQApplication::lastMessageWasSuppressed() noexcept
{
  return t_lastSuppressed;
}
#else
bool SafeQApplication::lastMessageWasSuppressed() noexcept
{
  return false;
}
#endif

void SafeQApplication::DebugOutput(
    QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
  auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
  auto basename = basename_arr.constData();
  FILE* out_file = stderr;
#if defined(_MSC_VER)
  static LogFile logger;
  out_file = logger.desc();
#endif

#if defined(__EMSCRIPTEN__)
  // Never throttle a fatal: it is the last thing that will ever be printed.
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

  {
    static const char* const prefixes[]
        = {"Debug", "Warning", "Critical", "Fatal", "Info"};
    // QtDebugMsg=0, QtWarningMsg=1, QtCriticalMsg=2, QtFatalMsg=3, QtInfoMsg=4
    const int idx = int(type);
    const char* prefix
        = (idx >= 0 && idx <= int(QtInfoMsg)) ? prefixes[idx] : "Log";
    const QByteArray line = QStringLiteral("%1: %2 (%3:%4)")
                                .arg(
                                    QString::fromUtf8(prefix), msg,
                                    QString::fromUtf8(basename))
                                .arg(context.line)
                                .toUtf8();
    emitLine(type, line.constData());

    if(type == QtFatalMsg)
    {
      SCORE_BREAKPOINT;
      std::terminate();
    }
    return;
  }
#endif

  QByteArray localMsg = msg.toLocal8Bit();
  switch(type)
  {
    case QtDebugMsg:
      fprintf(
          out_file, "Debug: %s (%s:%u)\n", localMsg.constData(), basename, context.line);
      break;
    case QtInfoMsg:
      fprintf(
          out_file, "Info: %s (%s:%u)\n", localMsg.constData(), basename, context.line);
      break;
    case QtWarningMsg:
      fprintf(
          out_file, "Warning: %s (%s:%u)\n", localMsg.constData(), basename,
          context.line);
      break;
    case QtCriticalMsg:
      fprintf(
          out_file, "Critical: %s (%s:%u)\n", localMsg.constData(), basename,
          context.line);
      break;
    case QtFatalMsg:
      fprintf(
          out_file, "Fatal: %s (%s:%u)\n", localMsg.constData(), basename, context.line);
      SCORE_BREAKPOINT;
      std::terminate();
  }
  fflush(out_file);
}

Q_GLOBAL_STATIC(QUrl, g_next_help_url_to_open);
Q_GLOBAL_STATIC(QTimer, g_next_help_url_to_open_timer);
static void open_help_url(const QUrl& u)
{
  if(g_next_help_url_to_open.isDestroyed())
    return;

  *g_next_help_url_to_open = u;
  g_next_help_url_to_open_timer->stop();
  g_next_help_url_to_open_timer->setSingleShot(true);
  QObject::disconnect(&*g_next_help_url_to_open_timer, &QTimer::timeout, qApp, nullptr);
  QObject::connect(&*g_next_help_url_to_open_timer, &QTimer::timeout, qApp, [] {
    QDesktopServices::openUrl(*g_next_help_url_to_open);
  });

  g_next_help_url_to_open_timer->start(15);
}

static void open_help_url(QGraphicsItem* item)
{
  if(!item)
    return;

  if(auto url = getItemHelpUrl(item->type()); !url.isEmpty())
  {
    open_help_url(url);
  }
  else if(auto data = item->data(0xF1); data.isValid())
  {
    open_help_url(data.toUrl());
  }
  else
  {
    open_help_url(item->parentItem());
  }
}

static void process_help_event(QObject* receiver, QEvent* event)
{
  if(auto res = receiver->property("help_url"); res.isValid())
  {
    auto url = res.value<QUrl>();
    open_help_url(url);
  }
  else if(auto gv = qobject_cast<QGraphicsView*>(receiver))
  {
    auto pos = QCursor::pos();
    auto pt = gv->viewport()->mapFromGlobal(pos);
    open_help_url(gv->itemAt(pt));
  }
  else if(auto gs = qobject_cast<QGraphicsScene*>(receiver))
  {
    auto pos = QCursor::pos();
    auto gvs = gs->views();
    if(!gvs.empty())
    {
      auto gv = gvs[0];
      auto pt = gv->mapFromGlobal(pos);
      open_help_url(gv->itemAt(pt));
    }
  }
  else
  {
    open_help_url(QUrl("https://ossia.io/score-docs"));
  }
}

bool SafeQApplication::notify(QObject* receiver, QEvent* event)
{
#if !defined(SCORE_DEBUG)
  try
  {
#endif
    if(event->type() == QEvent::KeyPress)
    {
      auto ev = (QKeyEvent*)(event);
      if(ev->key() == Qt::Key_F1)
        process_help_event(receiver, event);
    }
    return QApplication::notify(receiver, event);
#if !defined(SCORE_DEBUG)
  }
  catch(std::exception& e)
  {
    thread_local bool reentr = false;
    if(this->thread() != QThread::currentThread() || reentr)
    {
      qDebug() << "Internal error: " << e.what();
    }
    else
    {
      reentr = true;
      inform(QObject::tr("Internal error: ") + e.what());
      reentr = false;
    }
  }
  catch(...)
  {
    thread_local bool reentr = false;
    if(this->thread() != QThread::currentThread() || reentr)
    {
      qDebug() << "Internal error: ";
    }
    else
    {
      reentr = true;
      inform(QObject::tr("Internal error: "));
      reentr = false;
    }
  }

  return false;
#endif
}

bool SafeQApplication::event(QEvent* ev)
{
  switch((int)ev->type())
  {
    case QEvent::FileOpen: {
      auto loadString = static_cast<QFileOpenEvent*>(ev)->file();
#if defined(__APPLE__)
      // Used for the case when the user double-clicks something
      // with score not yet open, thus it's too early when the event
      // is processed
      this->fileToOpen = loadString;
#endif
      fileOpened(loadString);
      return true;
    }
    case QEvent::HelpRequest:
      return QApplication::event(ev);
    default:
      return QApplication::event(ev);
  }
}

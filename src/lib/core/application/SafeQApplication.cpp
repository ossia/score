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
#include <QThread>

#include <wobjectimpl.h>
W_OBJECT_IMPL(SafeQApplication)

SafeQApplication::~SafeQApplication() { }

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

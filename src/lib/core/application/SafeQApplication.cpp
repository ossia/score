// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SafeQApplication.hpp"

#include <score/tools/Debug.hpp>
#include <score/tools/std/Invoke.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QFileOpenEvent>
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

#if(QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
      fprintf(
          out_file, "Info: %s (%s:%u)\n", localMsg.constData(), basename, context.line);
      break;
#endif
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

#if !defined(SCORE_DEBUG)
bool SafeQApplication::notify(QObject* receiver, QEvent* event)
{
  try
  {
    return QApplication::notify(receiver, event);
  }
  catch(std::exception& e)
  {
    if(this->thread() != QThread::currentThread())
    {
      qDebug() << "Internal error: " << e.what();
    }
    else
    {
      inform(QObject::tr("Internal error: ") + e.what());
    }
  }
  catch(...)
  {
    if(this->thread() != QThread::currentThread())
    {
      qDebug() << "Internal error: ";
    }
    else
    {
      inform(QObject::tr("Internal error: "));
    }
  }

  return false;
}
#endif

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
    default:
      return QApplication::event(ev);
  }
}

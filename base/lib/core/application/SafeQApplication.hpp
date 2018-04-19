#pragma once
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <cstdio>
#include <wobjectdefs.h>

#ifdef __APPLE__
#  include <QFileOpenEvent>
#endif
#include <score/tools/Todo.hpp>
#include <score_lib_base_export.h>

/**
 * @brief C++ abstraction over fopen/fclose.
 *
 * Used to save std::cerr / std::cout to a file in Windows
 * which does not have a console for GUI programs.
 */
class SCORE_LIB_BASE_EXPORT LogFile
{
public:
  LogFile() : fd{fopen("score.log", "a")}
  {
  }

  FILE* desc() const
  {
    return fd;
  }
  ~LogFile()
  {
    fclose(fd);
  }

private:
  FILE* fd{};
};

/**
 * @brief Wrapper over QApplication
 *
 * Prevents an app crash in case of an internal error.
 * Disabled for debugging, because it makes getting the stack
 * trace harder.
 */
class SCORE_LIB_BASE_EXPORT SafeQApplication final : public QApplication
{
  W_OBJECT(SafeQApplication)
public:
  SafeQApplication(int& argc, char** argv) : QApplication{argc, argv}
  {
#if defined(SCORE_DEBUG)
    qInstallMessageHandler(DebugOutput);
#endif
  }

  ~SafeQApplication();
#if defined(SCORE_DEBUG)
  static void DebugOutput(
      QtMsgType type, const QMessageLogContext& context, const QString& msg)
  {
    auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
    auto basename = basename_arr.constData();
    FILE* out_file = stderr;
#  if defined(_MSC_VER)
    static LogFile logger;
    out_file = logger.desc();
#  endif
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type)
    {
      case QtDebugMsg:
        fprintf(
            out_file, "Debug: %s (%s:%u)\n", localMsg.constData(), basename,
            context.line);
        break;

#  if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
      case QtInfoMsg:
        fprintf(
            out_file, "Info: %s (%s:%u)\n", localMsg.constData(), basename,
            context.line);
        break;
#  endif
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
            out_file, "Fatal: %s (%s:%u)\n", localMsg.constData(), basename,
            context.line);
        SCORE_BREAKPOINT;
        std::terminate();
    }
    fflush(out_file);
  }
#else
  void inform(const QString& str)
  {
    QMessageBox::information(
        QApplication::activeWindow(), "", str, QMessageBox::Ok);
  }
  bool notify(QObject* receiver, QEvent* event) override
  {
    try
    {
      return QApplication::notify(receiver, event);
    }
    catch (std::exception& e)
    {
      inform(QObject::tr("Internal error: ") + e.what());
    }
    catch (...)
    {
      inform(QObject::tr("Internal error."));
    }

    return false;
  }
#endif

#ifdef __APPLE__
  bool event(QEvent* ev) override;
#endif

  void fileOpened(const QString& opened) W_SIGNAL(fileOpened, opened)
};

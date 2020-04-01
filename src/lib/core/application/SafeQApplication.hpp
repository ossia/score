#pragma once
#include <QApplication>
#include <QMessageBox>

#include <cstdio>
#include <verdigris>

#ifdef __APPLE__
#endif

#include <score_lib_base_export.h>
#include <score/widgets/MessageBox.hpp>

/**
 * @brief C++ abstraction over fopen/fclose.
 *
 * Used to save std::cerr / std::cout to a file in Windows
 * which does not have a console for GUI programs.
 */
class SCORE_LIB_BASE_EXPORT LogFile
{
public:
  LogFile() : fd{fopen("score.log", "a")} {}

  FILE* desc() const { return fd; }
  ~LogFile() { fclose(fd); }

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
  static void DebugOutput(
      QtMsgType type,
      const QMessageLogContext& context,
      const QString& msg);

#if !defined(SCORE_DEBUG)
  void inform(const QString& str)
  {
    score::information(
        QApplication::activeWindow(), "", str);
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

  bool event(QEvent* ev) override;

  void fileOpened(const QString& opened)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, fileOpened, opened)
};

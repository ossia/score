#pragma once
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <cstdio>

#ifdef __APPLE__
#include <QFileOpenEvent>
#endif
#include <iscore/tools/Todo.hpp>
#include <iscore_lib_base_export.h>

class ISCORE_LIB_BASE_EXPORT LogFile
{
public:
	LogFile():
		fd{ fopen("i-score.log", "a") }
	{
	}

	FILE* desc() const { return fd; }
	~LogFile()
	{
		fclose(fd);
	}
private:
	FILE* fd{};
};
/**
 * @brief The SafeQApplication class
 *
 * Prevents an app crash in case of an internal error.
 * Disabled for debugging, because it makes getting the stack
 * trace harder.
 */
class ISCORE_LIB_BASE_EXPORT SafeQApplication final : public QApplication
{
        Q_OBJECT
    public:
        SafeQApplication(int& argc, char** argv):
            QApplication{argc, argv}
        {
#if defined(ISCORE_DEBUG)
            qInstallMessageHandler(DebugOutput);
#endif
        }

        ~SafeQApplication();
#if defined(ISCORE_DEBUG)
		static void DebugOutput(
			QtMsgType type,
			const QMessageLogContext &context,
			const QString &msg)
		{
			auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
			auto basename = basename_arr.constData();
			FILE* out_file = stderr;
#if defined(_MSC_VER)
			static LogFile logger;
			out_file = logger.desc();
#endif
            QByteArray localMsg = msg.toLocal8Bit();
            switch (type) {
                case QtDebugMsg:
                    fprintf(out_file, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
                case QtInfoMsg:
                    fprintf(out_file, "Info: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
#endif
                case QtWarningMsg:
                    fprintf(out_file, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
                case QtCriticalMsg:
                    fprintf(out_file, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
                case QtFatalMsg:
                    fprintf(out_file, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    ISCORE_BREAKPOINT;
                    std::terminate();
            }
        }
#else
        void inform(const QString& str)
        {
            QMessageBox::information(
                        QApplication::activeWindow(), "", str, QMessageBox::Ok);
        }
        bool notify(QObject * receiver, QEvent * event) override
        {
            try
            {
                return QApplication::notify(receiver, event);
            }
            catch(std::exception& e)
            {
                inform(QObject::tr("Internal error: ") + e.what());
            }
            catch(...)
            {
                inform(QObject::tr("Internal error."));
            }

            return false;
        }
#endif

#ifdef __APPLE__
        bool event(QEvent *ev) override
        {
            bool eaten;
            switch (ev->type())
            {
                case QEvent::FileOpen:
                {
                    auto loadString = static_cast<QFileOpenEvent *>(ev)->file();
                    emit fileOpened(loadString);
                    eaten = true;
                    break;
                }
                default:
                    return QApplication::event(ev);
            }
            return eaten;
        }
#endif

    signals:
        void fileOpened(const QString&);
};

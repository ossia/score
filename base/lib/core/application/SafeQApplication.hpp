#pragma once
#include <QApplication>
#include <QMessageBox>
#include <cstdio>

class TTException {
        const char*	reason;
    public:
        TTException(const char* aReason)
            : reason(aReason)
        {}

        const char* getReason()
        {
            return reason;
        }
};

/**
 * @brief The SafeQApplication class
 *
 * Prevents an app crash in case of an internal error.
 * Disabled for debugging, because it makes getting the stack
 * trace harder.
 */
class SafeQApplication : public QApplication
{
    public:
        SafeQApplication(int& argc, char** argv):
            QApplication{argc, argv}
        {
#if defined(ISCORE_DEBUG)
            qInstallMessageHandler(DebugOutput);
#endif
        }

#if defined(ISCORE_DEBUG)
        static void DebugOutput(
                QtMsgType type,
                const QMessageLogContext &context,
                const QString &msg)
        {
            auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
            auto basename = basename_arr.constData();

            QByteArray localMsg = msg.toLocal8Bit();
            switch (type) {
                case QtDebugMsg:
                    fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
                case QtInfoMsg:
                    fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
#endif
                case QtWarningMsg:
                    fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
                case QtCriticalMsg:
                    fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
                    break;
                case QtFatalMsg:
                    fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
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
            catch(TTException& e)
            {
                inform(QObject::tr("Internal error: ") + e.getReason());
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
};

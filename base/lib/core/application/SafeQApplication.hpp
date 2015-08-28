#pragma once
#include <QApplication>
#include <QMessageBox>

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
        using QApplication::QApplication;
#if !defined(ISCORE_DEBUG)

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

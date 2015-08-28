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

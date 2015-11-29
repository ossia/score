#pragma once
#include <qobject.h>
#include <qstring.h>

class QAction;
class QDialog;
class QListView;
class QWidget;

class ZeroconfBrowser : public QObject
{
        Q_OBJECT
    public:
        ZeroconfBrowser(
                const QString& service,
                QWidget* parent);
        QAction* makeAction();

    signals:
        // ip, port
        void sessionSelected(QString, int);

    public slots:
        void accept();
        void reject();

    private:
        QDialog* m_dialog{};
        QListView* m_list{};
};

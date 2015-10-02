#pragma once
#include <QObject>

class QAction;
class QDialog;
class QListView;
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

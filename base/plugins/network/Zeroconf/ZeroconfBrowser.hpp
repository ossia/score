#pragma once
#include <QObject>

class QAction;
class QDialog;
class QListView;
class ZeroconfBrowser : public QObject
{
        Q_OBJECT
    public:
        ZeroconfBrowser(QObject* parent);
        QAction* makeAction();

    signals:
        void sessionSelected(QString ip, int port);

    public slots:
        void accept();
        void reject();

    private:
        QDialog* m_dialog{};
        QListView* m_list{};
};

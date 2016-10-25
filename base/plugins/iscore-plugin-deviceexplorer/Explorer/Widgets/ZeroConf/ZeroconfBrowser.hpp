#pragma once
#include <QObject>
#include <QString>
#include <iscore_plugin_deviceexplorer_export.h>

class QAction;
class QDialog;
class QListView;
class QWidget;

class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ZeroconfBrowser :
    public QObject
{
        Q_OBJECT
    public:
        ZeroconfBrowser(
                const QString& service,
                QWidget* parent);
        QAction* makeAction();

    signals:
        // ip, port, other data
        void sessionSelected(QString, int, QMap<QString, QByteArray>);

    public slots:
        void accept();
        void reject();

    private:
        QDialog* m_dialog{};
        QListView* m_list{};
};

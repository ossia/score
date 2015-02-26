#pragma once
#include <QCompleter>
class DeviceExplorerModel;
class DeviceCompleter : public QCompleter
{
    public:
        DeviceCompleter (DeviceExplorerModel* model, QObject* parent);

        QString pathFromIndex (const QModelIndex& index) const;
        QStringList splitPath (const QString& path) const;
};

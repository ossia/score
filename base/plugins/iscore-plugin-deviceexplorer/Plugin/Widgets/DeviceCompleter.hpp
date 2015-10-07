#pragma once
#include <QCompleter>
class DeviceExplorerModel;

/**
 * @brief The DeviceCompleter class
 *
 * A completer that uses a DeviceExplorerModel as data.
 * It is used to input addresses for instance, and will complete with
 * existing ones.
 */
class DeviceCompleter : public QCompleter
{
    public:
        DeviceCompleter(DeviceExplorerModel* model, QObject* parent);

        QString pathFromIndex(const QModelIndex& index) const override;
        QStringList splitPath(const QString& path) const override;
};

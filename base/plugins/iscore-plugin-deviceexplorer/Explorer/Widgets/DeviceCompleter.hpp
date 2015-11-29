#pragma once
#include <qcompleter.h>
#include <qstring.h>
#include <qstringlist.h>

class DeviceExplorerModel;
class QModelIndex;
class QObject;

/**
 * @brief The DeviceCompleter class
 *
 * A completer that uses a DeviceExplorerModel as data.
 * It is used to input addresses for instance, and will complete with
 * existing ones.
 */
class DeviceCompleter final : public QCompleter
{
    public:
        DeviceCompleter(DeviceExplorerModel* model, QObject* parent);

        QString pathFromIndex(const QModelIndex& index) const override;
        QStringList splitPath(const QString& path) const override;
};

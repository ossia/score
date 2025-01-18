#pragma once
#include <QCompleter>
#include <QString>
#include <QStringList>

#include <score_lib_device_export.h>

class QModelIndex;
class QObject;
namespace Device
{
class NodeBasedItemModel;
}
namespace Device
{

/**
 * @brief The DeviceCompleter class
 *
 * A completer that uses a DeviceExplorerModel as data.
 * It is used to input addresses for instance, and will complete with
 * existing ones.
 */
class SCORE_LIB_DEVICE_EXPORT DeviceCompleter final : public QCompleter
{
public:
  DeviceCompleter(Device::NodeBasedItemModel& model, QObject* parent);

  QString pathFromIndex(const QModelIndex& index) const override;
  QStringList splitPath(const QString& path) const override;
};
}

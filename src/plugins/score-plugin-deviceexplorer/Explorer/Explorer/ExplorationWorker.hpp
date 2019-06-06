#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <QObject>
#include <QString>

#include <verdigris>

namespace Device
{
class DeviceInterface;
}

namespace Explorer
{
/**
 * @brief The ExplorationWorker class
 *
 * Used as a thread worker to perform refreshing of a remote device without GUI
 * interruption. See DeviceExplorerWidget::refresh() for usage.
 */
class ExplorationWorker final : public QObject
{
  W_OBJECT(ExplorationWorker)
public:
  Device::DeviceInterface& dev;
  Device::Node node; // Result

  explicit ExplorationWorker(Device::DeviceInterface& dev);

public:
  void finished() W_SIGNAL(finished);
  void failed(QString arg_1) W_SIGNAL(failed, arg_1);
};
}

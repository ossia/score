#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <score_plugin_deviceexplorer_export.h>
namespace Device
{
class DeviceInterface;
}
namespace Explorer
{
class DeviceExplorerModel;
class DeviceExplorerWidget;
class ListeningHandler;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningManager : public QObject
{
  DeviceExplorerModel& m_model;
  const DeviceExplorerWidget& m_widget;

public:
  ListeningManager(DeviceExplorerModel&, const DeviceExplorerWidget&);

  void enableListening(Device::Node& node);
  void setListening(const QModelIndex& idx, bool b);
  void resetListening(Device::Node& idx);

  // Will do it for all devices
  void stopListening();

  // Sets the listening state with the expanded nodes
  void setDeviceWidgetListening();

private:
  void disableListening_rec(
      const Device::Node& node, Device::DeviceInterface&, ListeningHandler& lm);
  void enableListening_rec(
      const QModelIndex& index, Device::DeviceInterface&, ListeningHandler& lm);

  //! Null when the node names a device with no implementation behind it: a
  //! protocol this build does not have, or a score that runs on another
  //! machine. There is nothing to listen to in either case.
  Device::DeviceInterface* deviceFromNode(const Device::Node&);
  Device::DeviceInterface* deviceFromProxyModelIndex(const QModelIndex&);
  Device::DeviceInterface* deviceFromModelIndex(const QModelIndex& idx);

  Device::Node& nodeFromProxyModelIndex(const QModelIndex&);
  Device::Node& nodeFromModelIndex(const QModelIndex&);

  ListeningHandler& m_handler;
};
}

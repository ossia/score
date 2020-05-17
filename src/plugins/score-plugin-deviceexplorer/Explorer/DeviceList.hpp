#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <QString>

#include <score_plugin_deviceexplorer_export.h>

#include <functional>
#include <vector>
#include <verdigris>

namespace Device
{

/**
 * @brief The DeviceList class
 *
 * Once a device is added with addDevice, the DeviceList
 * has ownership.
 *
 * This is not the case for the local device, which is owned
 * by LocalTreeDocumentPlugin
 */
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceList : public QObject
{
  W_OBJECT(DeviceList)
public:
  DeviceInterface& device(const QString& name) const;
  DeviceInterface& device(const Device::Node& name) const;

  DeviceInterface* findDevice(const QString& name) const;

  void addDevice(DeviceInterface* dev);
  void removeDevice(const QString& name);

  void apply(std::function<void(Device::DeviceInterface&)>);
  void setLogging(bool);

  void setLocalDevice(DeviceInterface*);
  void setAudioDevice(DeviceInterface* dev) { m_audioDevice = dev; }
  DeviceInterface* localDevice() const { return m_localDevice; }
  DeviceInterface* audioDevice() const { return m_audioDevice; }
  const std::vector<DeviceInterface*>& devices() const;

public:
  void logInbound(const QString& arg_1) const
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, logInbound, arg_1)
  void logOutbound(const QString& arg_1) const
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, logOutbound, arg_1)
  void logError(const QString& arg_1) const
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, logError, arg_1)

  void deviceAdded(DeviceInterface& dev) const
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, deviceAdded, dev)
  void deviceRemoved(DeviceInterface& dev) const
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, deviceRemoved, dev)

private:
  std::vector<DeviceInterface*> m_devices;
  DeviceInterface *m_localDevice{}, *m_audioDevice{};
  bool m_logging = false;
};
}

W_REGISTER_ARGTYPE(Device::DeviceInterface&)

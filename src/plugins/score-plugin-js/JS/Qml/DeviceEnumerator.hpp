#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <QQmlListProperty>

#include <unordered_map>
#include <verdigris>

namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Device
{
struct DeviceSettings;
class DeviceList;
class ProtocolFactory;
class DeviceEnumerator;
}

namespace JS
{
struct DeviceIdentifier
{
  W_GADGET(DeviceIdentifier)
public:
  QString name{};
  Device::DeviceSettings settings{};
  Device::ProtocolFactory* protocol{};

  W_PROPERTY(QString, MEMBER name)
  W_PROPERTY(Device::DeviceSettings, MEMBER settings)
};

class GlobalDeviceEnumerator : public QObject
{
  W_OBJECT(GlobalDeviceEnumerator)

public:
  explicit GlobalDeviceEnumerator();
  ~GlobalDeviceEnumerator();

  void setContext(const score::DocumentContext* doc);
  W_SLOT(setContext)

  void deviceAdded(
      Device::ProtocolFactory* factory, const QString& name,
      Device::DeviceSettings settings) W_SIGNAL(deviceAdded, factory, name, settings)
  void deviceRemoved(Device::ProtocolFactory* factory, const QString& name)
      W_SIGNAL(deviceRemoved, factory, name)
  // QList<Device::DeviceSettings> devices();
  QQmlListProperty<JS::DeviceIdentifier> devices();

  W_PROPERTY(QQmlListProperty<JS::DeviceIdentifier>, devices READ devices)

  bool enumerate() { return m_enumerate; }
  void setEnumerate(bool b);
  void enumerateChanged(bool b) W_SIGNAL(enumerateChanged, b)
  W_PROPERTY(bool, enumerate READ enumerate WRITE setEnumerate NOTIFY enumerateChanged)

private:
  void reprocess();
  const score::DocumentContext* doc{};

  std::unordered_map<
      Device::ProtocolFactory*, std::vector<std::pair<QString, Device::DeviceSettings>>>
      m_known_devices;
  std::unordered_map<Device::ProtocolFactory*, Device::DeviceEnumerators>
      m_current_enums;

  std::vector<DeviceIdentifier*> m_raw_list;

  bool m_enumerate{};
};
}

Q_DECLARE_METATYPE(JS::GlobalDeviceEnumerator*)
W_REGISTER_ARGTYPE(Device::ProtocolFactory*)

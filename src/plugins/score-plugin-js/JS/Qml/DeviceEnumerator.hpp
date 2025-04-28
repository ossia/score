#pragma once

#include <State/Address.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QQmlListProperty>

#include <unordered_map>
#include <verdigris>

namespace ossia::net
{
class node_base;
class parameter_base;
}
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
struct DeviceIdentifier : public QObject
{
  W_OBJECT(DeviceIdentifier)
public:
  explicit DeviceIdentifier(
      QString cat, QString a, Device::DeviceSettings b, Device::ProtocolFactory* c)
      : category{std::move(cat)}
      , name{std::move(a)}
      , settings{std::move(b)}
      , protocol{c}
  {
  }
  QString category;
  QString name;
  Device::DeviceSettings settings;
  Device::ProtocolFactory* protocol{};

  W_PROPERTY(QString, category MEMBER category)
  W_PROPERTY(QString, name MEMBER name)
  W_PROPERTY(Device::DeviceSettings, settings MEMBER settings)
  W_PROPERTY(Device::ProtocolFactory*, protocol MEMBER protocol)
};

class GlobalDeviceEnumerator : public QObject
{
  W_OBJECT(GlobalDeviceEnumerator)

public:
  explicit GlobalDeviceEnumerator();
  //  explicit GlobalDeviceEnumerator(const QString& uuid);
  ~GlobalDeviceEnumerator();

  void setContext(const score::DocumentContext* doc);
  W_SLOT(setContext)

  void deviceAdded(
      Device::ProtocolFactory* factory, const QString& category, const QString& name,
      Device::DeviceSettings settings)
      W_SIGNAL(deviceAdded, factory, category, name, settings)
  void deviceRemoved(Device::ProtocolFactory* factory, const QString& name)
      W_SIGNAL(deviceRemoved, factory, name)
  QQmlListProperty<JS::DeviceIdentifier> devices();

  W_PROPERTY(QQmlListProperty<JS::DeviceIdentifier>, devices READ devices)

  bool enumerate() { return m_enumerate; }
  void setEnumerate(bool b);
  void enumerateChanged(bool b) W_SIGNAL(enumerateChanged, b)
  W_PROPERTY(bool, enumerate READ enumerate WRITE setEnumerate NOTIFY enumerateChanged)

  QString deviceType() { return m_uuid; }
  void setDeviceType(const QString& b);
  void deviceTypeChanged(const QString& b) W_SIGNAL(deviceTypeChanged, b)
  W_PROPERTY(
      QString, deviceType READ deviceType WRITE setDeviceType NOTIFY deviceTypeChanged)

private:
  void reprocess();
  const score::DocumentContext* doc{};

  std::unordered_map<
      Device::ProtocolFactory*,
      std::vector<std::tuple<QString, QString, Device::DeviceSettings>>>
      m_known_devices;
  std::unordered_map<Device::ProtocolFactory*, Device::DeviceEnumerators>
      m_current_enums;

  std::vector<DeviceIdentifier*> m_raw_list;
  QString m_uuid;
  Device::ProtocolFactory::ConcreteKey m_deviceType{};
  bool m_enumerate{};
};

class DeviceListener
    : public QObject
    , public Nano::Observer
{
  W_OBJECT(DeviceListener)

public:
  explicit DeviceListener();
  ~DeviceListener();

  QString deviceName() { return m_uuid; }
  void setDeviceName(const QString& b);
  void deviceNameChanged(const QString& b) W_SIGNAL(deviceNameChanged, b)
  W_PROPERTY(
      QString, deviceName READ deviceName WRITE setDeviceName NOTIFY deviceNameChanged)

  QString deviceType() { return m_uuid; }
  void setDeviceType(const QString& b);
  void deviceTypeChanged(const QString& b) W_SIGNAL(deviceTypeChanged, b)
  W_PROPERTY(
      QString, deviceType READ deviceType WRITE setDeviceType NOTIFY deviceTypeChanged)

  bool listen() { return m_listen; }
  void setListen(bool b);
  void listenChanged(bool b) W_SIGNAL(listenChanged, b)
  W_PROPERTY(bool, listen READ listen WRITE setListen NOTIFY listenChanged)

  // To QML
  void message(const QString& address, QVariant value) W_SIGNAL(message, address, value);
  void parameterCreated(const QString& address) W_SIGNAL(parameterCreated, address);

private:
  const score::DocumentContext* ctx{};
  void init();
  void on_deviceAdded(Device::DeviceInterface& dev);
  void on_nodeCreated(const ossia::net::node_base&);
  void on_parameterCreated(const ossia::net::parameter_base&);

  QString m_name;
  QString m_uuid;
  Device::ProtocolFactory::ConcreteKey m_deviceType{};

  bool m_listen{};
};

QString addressFromParameter(const ossia::net::parameter_base& p);
}

Q_DECLARE_METATYPE(JS::DeviceListener*)
Q_DECLARE_METATYPE(JS::GlobalDeviceEnumerator*)
W_REGISTER_ARGTYPE(JS::DeviceListener*)
W_REGISTER_ARGTYPE(Device::ProtocolFactory*)

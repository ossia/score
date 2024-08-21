#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QObject>

#include <score_plugin_protocols_export.h>

#include <verdigris>
namespace ossia
{
class value;
}

namespace ossia::net
{
class parameter_base;
class node_base;
class device_base;
struct simpleio_protocol;
}

namespace Protocols::SimpleIO
{
struct Port;
struct ADC_impl
{
  int fd{};
  float value{};
};
struct DAC_impl
{
  int fd{};
  float value{};
};
struct PWM_impl
{
  int fd{};
};
struct GPIO_impl
{
  int fd{};
  int value{};
};

class HardwareDevice
    : public QObject
    , public score::SerializableInterface<HardwareDevice>
{
  W_OBJECT(HardwareDevice)
  SCORE_SERIALIZE_FRIENDS
public:
  explicit HardwareDevice(QObject* parent) noexcept;
  HardwareDevice(DataStream::Deserializer& vis, QObject* parent);
  HardwareDevice(JSONObject::Deserializer& vis, QObject* parent);
  ~HardwareDevice();

  virtual std::unique_ptr<HardwareDevice> clone() noexcept = 0;

  virtual void setupConfiguration(QWidget* widg) = 0;
  virtual std::vector<Protocols::SimpleIO::Port> getConfiguration() const noexcept = 0;
  virtual void loadConfiguration(const std::vector<Protocols::SimpleIO::Port>& hardware)
      = 0;

  using used_parameters = ossia::small_pod_vector<ossia::net::parameter_base*, 8>;
  virtual used_parameters setupDevice(
      ossia::net::simpleio_protocol& proto, ossia::net::node_base& dev,
      const QString& name, const QString& path)
      = 0;

  virtual void teardownDevice() = 0;

  virtual void pull(ossia::net::parameter_base& v) = 0;
  virtual void push(const ossia::net::parameter_base& p, const ossia::value& v) = 0;
};

class HardwareDeviceFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(HardwareDeviceFactory, "8c8d93d9-c46c-4d24-8efe-c7868dc74dc3")
public:
  virtual ~HardwareDeviceFactory();

  virtual QString prettyName() const noexcept = 0;
  virtual HardwareDevice* make(QObject* parent) = 0;
  virtual HardwareDevice* load(const VisitorVariant& data, QObject* parent) = 0;
};

class HardwareDeviceFactoryList final
    : public score::InterfaceList<HardwareDeviceFactory>
{
public:
  using object_type = HardwareDevice;
  object_type* loadMissing(const VisitorVariant& vis, QObject* parent) const
  {
    SCORE_TODO;
    return nullptr;
  }
};
}

OBJECTKEY_METADATA(
    SCORE_PLUGIN_PROTOCOLS_EXPORT, Protocols::SimpleIO::HardwareDevice, "HardwareDevice")

Q_DECLARE_METATYPE(Protocols::SimpleIO::HardwareDeviceFactory::ConcreteKey)
W_REGISTER_ARGTYPE(Protocols::SimpleIO::HardwareDeviceFactory::ConcreteKey)

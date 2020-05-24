#pragma once
#include <score/plugins/Interface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>
#include <QVariant>

#include <score_lib_device_export.h>

struct VisitorVariant;

namespace Device
{
struct DeviceSettings;
struct AddressSettings;
class DeviceInterface;
class ProtocolSettingsWidget;
class AddressDialog;
class SCORE_LIB_DEVICE_EXPORT DeviceEnumerator : public QObject
{
  W_OBJECT(DeviceEnumerator)
public:
  virtual ~DeviceEnumerator();

  virtual void enumerate(std::function<void(const Device::DeviceSettings&)>) const = 0;

  void deviceAdded(const Device::DeviceSettings& s) E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, deviceAdded, s)
  void deviceRemoved(const QString& s) E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, deviceRemoved, s)
};

class SCORE_LIB_DEVICE_EXPORT ProtocolFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ProtocolFactory, "3f69d72e-318d-42dc-b48c-a806036592f1")

public:
  virtual ~ProtocolFactory();
  struct StandardCategories {
    static const constexpr auto osc = "OSC";
    static const constexpr auto audio = "Audio";
    static const constexpr auto video = "Video";
    static const constexpr auto web = "Web";
    static const constexpr auto hardware = "Hardware";
    static const constexpr auto lights = "Lights";
    static const constexpr auto util = "Utilities";
  };

  virtual QString prettyName() const noexcept = 0;
  virtual QString category() const noexcept = 0;

  /** The one with the highest priority
   * will show up first in the protocol list */
  virtual int visualPriority() const noexcept;

  virtual DeviceEnumerator*
  getEnumerator(
      const score::DocumentContext& ctx
  ) const = 0;

  virtual DeviceInterface*
  makeDevice(
      const Device::DeviceSettings& settings,
      const score::DocumentContext& ctx
  ) = 0;

  virtual ProtocolSettingsWidget* makeSettingsWidget() = 0;

  virtual AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*
  ) = 0;
  virtual AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*
  ) = 0;

  virtual const Device::DeviceSettings& defaultSettings() const noexcept = 0;

  // Save
  virtual void
  serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const = 0;

  template <typename T>
  void
  serializeProtocolSpecificSettings_T(const QVariant& data, const VisitorVariant& visitor) const
  {
    score::serialize_dyn(visitor, data.value<T>());
  }

  // Load
  virtual QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const = 0;

  template <typename T>
  QVariant makeProtocolSpecificSettings_T(const VisitorVariant& vis) const
  {
    return QVariant::fromValue(score::deserialize_dyn<T>(vis));
  }

  // Returns true if the two devicesettings can coexist at the same time.
  virtual bool
  checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept = 0;
};
}

Q_DECLARE_METATYPE(UuidKey<Device::ProtocolFactory>)
W_REGISTER_ARGTYPE(UuidKey<Device::ProtocolFactory>)

#pragma once
#include <Device/Protocol/DeviceSettings.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/UuidKeySerialization.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>
#include <QVariant>

#include <score_lib_device_export.h>

class QUrl;
struct VisitorVariant;

namespace Explorer
{
class DeviceDocumentPlugin;
}
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

  virtual void
      enumerate(std::function<void(const QString&, const Device::DeviceSettings&)>) const
      = 0;

  void deviceAdded(const QString& n, const Device::DeviceSettings& s)
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, deviceAdded, n, s)
  void deviceRemoved(const QString& s)
      E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, deviceRemoved, s)

  void sort() E_SIGNAL(SCORE_LIB_DEVICE_EXPORT, sort)
};

using DeviceEnumerators = std::vector<std::pair<QString, Device::DeviceEnumerator*>>;

class SCORE_LIB_DEVICE_EXPORT ProtocolFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ProtocolFactory, "3f69d72e-318d-42dc-b48c-a806036592f1")

public:
  virtual ~ProtocolFactory();
  struct StandardCategories
  {
    static const constexpr auto osc = "Network";
    static const constexpr auto audio = "Audio";
    static const constexpr auto video = "Video";
    static const constexpr auto web = "Web";
    static const constexpr auto hardware = "Hardware";
    static const constexpr auto software = "Software";
    static const constexpr auto lights = "Lights";
    static const constexpr auto util = "Utilities";
  };

  enum Flags
  {
    EditingReloadsEverything = (1 << 0)
  };

  virtual Flags flags() const noexcept;

  virtual QString prettyName() const noexcept = 0;
  virtual QString category() const noexcept = 0;
  virtual QUrl manual() const noexcept;

  /** The one with the highest priority
   * will show up first in the protocol list */
  virtual int visualPriority() const noexcept;

  virtual DeviceEnumerators getEnumerators(const score::DocumentContext& ctx) const;

  virtual DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx)
      = 0;

  virtual ProtocolSettingsWidget* makeSettingsWidget() = 0;

  virtual AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx, QWidget*)
      = 0;
  virtual AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*)
      = 0;

  virtual const Device::DeviceSettings& defaultSettings() const noexcept = 0;

  // Save
  virtual void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const
      = 0;

  template <typename T>
  void serializeProtocolSpecificSettings_T(
      const QVariant& data, const VisitorVariant& visitor) const
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

  //! Returns true if the device can be instantiated.
  //! e.g. are the necessary ports available at the system level.
  //! Second argument is a map of all the used resources by other devices in the score.
  virtual bool checkResourcesAvailable(
      const Device::DeviceSettings& a, const DeviceResourceMap&) const noexcept;

  // Returns true if the two devicesettings can coexist at the same time.
  virtual bool checkCompatibility(
      const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
      = 0;
};
}

Q_DECLARE_METATYPE(UuidKey<Device::ProtocolFactory>)
W_REGISTER_ARGTYPE(UuidKey<Device::ProtocolFactory>)

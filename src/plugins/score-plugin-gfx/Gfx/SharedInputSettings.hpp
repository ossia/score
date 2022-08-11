#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Gfx/SharedInputSettings.hpp>

#include <QString>

#include <score_plugin_gfx_export.h>

#include <verdigris>

class QFormLayout;
class QSpinBox;
class QLineEdit;
namespace Gfx
{
struct SharedInputSettings
{
  QString path;
};

class SCORE_PLUGIN_GFX_EXPORT SharedInputProtocolFactory : public Device::ProtocolFactory
{
public:
  ~SharedInputProtocolFactory();

  QString category() const noexcept override;

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class SharedInputSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  SharedInputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  Device::DeviceSettings m_settings;
  QFormLayout* m_layout{};
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_shmPath{};
};

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::SharedInputSettings);
Q_DECLARE_METATYPE(Gfx::SharedInputSettings)
W_REGISTER_ARGTYPE(Gfx::SharedInputSettings)

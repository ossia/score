#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QString>

#include <score_plugin_gfx_export.h>

#include <verdigris>

class QFormLayout;
class QSpinBox;
class QLineEdit;
namespace Gfx
{
struct SharedOutputSettings
{
  QString path;
  int width{};
  int height{};
  double rate{};
};

class SCORE_PLUGIN_GFX_EXPORT SharedOutputProtocolFactory
    : public Device::ProtocolFactory
{
public:
  ~SharedOutputProtocolFactory();
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

  QString category() const noexcept override;

  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;
};

class SharedOutputSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  SharedOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QFormLayout* m_layout{};
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_shmPath{};
  QSpinBox* m_width{};
  QSpinBox* m_height{};
  QSpinBox* m_rate{};
};

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::SharedOutputSettings);
Q_DECLARE_METATYPE(Gfx::SharedOutputSettings)
W_REGISTER_ARGTYPE(Gfx::SharedOutputSettings)

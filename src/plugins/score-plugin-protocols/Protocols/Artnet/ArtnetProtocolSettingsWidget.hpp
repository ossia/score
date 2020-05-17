#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;
class QComboBox;

namespace Protocols
{

class ArtnetProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(ArtnetProtocolSettingsWidget)

public:
  ArtnetProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~ArtnetProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
};
}
#endif

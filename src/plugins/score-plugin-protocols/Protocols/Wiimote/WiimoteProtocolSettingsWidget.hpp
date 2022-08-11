#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;

namespace Protocols
{

class WiimoteProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(WiimoteProtocolSettingsWidget)

public:
  WiimoteProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~WiimoteProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
};

}

#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;
class QCheckBox;

namespace Protocols
{
class JoystickProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(JoystickProtocolSettingsWidget)

public:
  JoystickProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~JoystickProtocolSettingsWidget();

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
  QCheckBox* m_gamepad{};
  Device::DeviceSettings m_settings;
};
}

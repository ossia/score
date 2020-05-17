#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QLineEdit;
class QComboBox;

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

  void update_device_list();

protected:
  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_deviceChoice{};
};
}

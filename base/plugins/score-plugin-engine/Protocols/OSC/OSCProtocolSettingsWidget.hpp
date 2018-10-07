#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <wobjectdefs.h>

class QLineEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
{
class OSCProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(OSCProtocolSettingsWidget)

public:
  OSCProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_deviceNameEdit;
  QSpinBox* m_portOutputSBox;
  QSpinBox* m_portInputSBox;
  QLineEdit* m_localHostEdit;
};
}
}

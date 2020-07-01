#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>
#include <verdigris>

class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class RateWidget;
class OSCProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  OSCProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  QSpinBox* m_portOutputSBox{};
  QSpinBox* m_portInputSBox{};
  QLineEdit* m_localHostEdit{};
  RateWidget* m_rate{};
};
}

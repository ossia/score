#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
class QLineEdit;
class JSEdit;
class QSpinBox;
class QWidget;
class QComboBox;

namespace Engine
{
namespace Network
{
class SerialProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  SerialProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_name{};
  QComboBox* m_port{};
  JSEdit* m_codeEdit{};
};
}
}

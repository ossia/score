#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class QTextEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class HTTPProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  HTTPProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_deviceNameEdit{};
  QTextEdit* m_codeEdit{};
};
}

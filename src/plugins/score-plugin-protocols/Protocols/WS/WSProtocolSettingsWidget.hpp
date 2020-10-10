#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class QCodeEditor;
class QSpinBox;
class QWidget;

namespace Protocols
{
class WSProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  WSProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();
  void parseHost();

protected:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_addressNameEdit{};
  QCodeEditor* m_codeEdit{};
};
}

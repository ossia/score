#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class JSEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
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
  JSEdit* m_codeEdit{};
};
}
}

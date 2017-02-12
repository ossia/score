#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

class ZeroconfBrowser;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
{
class OSCQueryProtocolSettingsWidget final
    : public Device::ProtocolSettingsWidget
{
public:
  OSCQueryProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_localHostEdit{};

  ZeroconfBrowser* m_browser{};
};
}
}

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
class MinuitProtocolSettingsWidget final
    : public Device::ProtocolSettingsWidget
{
public:
  MinuitProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_deviceNameEdit{};
  QSpinBox* m_portInputSBox{};
  QSpinBox* m_portOutputSBox{};
  QLineEdit* m_localHostEdit{};
  QLineEdit* m_localNameEdit{};

  ZeroconfBrowser* m_browser{};
};
}
}

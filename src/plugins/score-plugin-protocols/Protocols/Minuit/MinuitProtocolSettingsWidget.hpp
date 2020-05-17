#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class ZeroconfBrowser;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class RateWidget;
class MinuitProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
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
  RateWidget* m_rate{};

  ZeroconfBrowser* m_browser{};
};
}

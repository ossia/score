#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
class QLineEdit;
class QWidget;

namespace Protocols
{
class PhidgetProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  PhidgetProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_name{};
};
}

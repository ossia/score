#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{
class LocalProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  LocalProtocolSettingsWidget(QWidget* parent = nullptr);

private:
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  QSpinBox* m_oscPort{};
  QSpinBox* m_wsPort{};
};
}

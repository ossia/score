#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <verdigris>

class QStackedLayout;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{

class BasicTCPWidget;
class WebsocketClientWidget;
class RateWidget;

class BitfocusProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit BitfocusProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  BitfocusSpecificSettings m_settings;
};
}

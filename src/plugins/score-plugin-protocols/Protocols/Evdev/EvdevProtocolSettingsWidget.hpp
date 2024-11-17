#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Evdev/EvdevSpecificSettings.hpp>

#include <verdigris>

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;

namespace Protocols
{

class EvdevProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(EvdevProtocolSettingsWidget)

public:
  EvdevProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~EvdevProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  Device::DeviceSettings m_settings;
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_host{};
  QSpinBox* m_port{};
};
}
#endif

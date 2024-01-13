#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/GPS/GPSSpecificSettings.hpp>

#include <verdigris>

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;

namespace Protocols
{

class GPSProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(GPSProtocolSettingsWidget)

public:
  GPSProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~GPSProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_host{};
  QSpinBox* m_port{};
};
}
#endif

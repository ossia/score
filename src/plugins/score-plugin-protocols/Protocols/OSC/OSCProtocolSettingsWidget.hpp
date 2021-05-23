#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <verdigris>

class QStackedLayout;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{

class UDPWidget;
class TCPWidget;
class UnixDatagramWidget;
class UnixStreamWidget;
class SerialWidget;
class WebsocketClientWidget;
class WebsocketServerWidget;

class RateWidget;
class OSCProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  OSCProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  Device::Node getDevice() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;
private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  RateWidget* m_rate{};
  QComboBox* m_transport{};
  QComboBox* m_oscVersion{};
  QStackedLayout* m_transportLayout{};

  UDPWidget* m_udp{};
  TCPWidget* m_tcp{};
  SerialWidget* m_serial{};
  UnixDatagramWidget* m_unix_dgram{};
  UnixStreamWidget* m_unix_stream{};
  WebsocketClientWidget* m_ws_client{};
  WebsocketServerWidget* m_ws_server{};

  OSCSpecificSettings m_settings;
};
}

#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <verdigris>

class QStackedLayout;
class QLineEdit;
class QSpinBox;
class QWidget;
class QFormLayout;

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

enum class OscProtocol
{
  UDP = 0,
  TCP = 1,
  Serial = 2,
  UnixDatagram = 3,
  UnixStream = 4,
  WSClient = 5,
  WSServer = 6
};

class OSCTransportWidget : public QWidget
{
public:
  explicit OSCTransportWidget(
      Device::ProtocolSettingsWidget& proto, QWidget* parent = nullptr);

  void setCurrentProtocol(OscProtocol index);
  ossia::net::osc_protocol_configuration configuration(OscProtocol index) const noexcept;
  OscProtocol setConfiguration(const ossia::net::osc_protocol_configuration& conf);

private:
  QStackedLayout* m_transportLayout{};

  UDPWidget* m_udp{};
  TCPWidget* m_tcp{};
  SerialWidget* m_serial{};
  UnixDatagramWidget* m_unix_dgram{};
  UnixStreamWidget* m_unix_stream{};
  WebsocketClientWidget* m_ws_client{};
  WebsocketServerWidget* m_ws_server{};
};

class OSCProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit OSCProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  Device::Node getDevice() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  RateWidget* m_rate{};
  QCheckBox* m_bonjour{};
  QComboBox* m_transport{};
  QComboBox* m_oscVersion{};
  OSCTransportWidget* m_transportWidget{};
  OSCSpecificSettings m_settings;
};
}

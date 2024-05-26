#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/CoAP/CoAPSpecificSettings.hpp>

#include <verdigris>

class QStackedLayout;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace Protocols
{

class UDPWidget;
class BasicTCPWidget;
class WebsocketClientWidget;
class RateWidget;

enum class CoapProtocol
{
  UDP = 0,
  TCP = 1,
  WSClient = 2,
};

class CoAPTransportWidget : public QWidget
{
public:
  explicit CoAPTransportWidget(
      Device::ProtocolSettingsWidget& proto, QWidget* parent = nullptr);

  void setCurrentProtocol(CoapProtocol index);
  ossia::net::coap_client_configuration configuration(CoapProtocol index) const noexcept;
  CoapProtocol setConfiguration(const ossia::net::coap_client_configuration& conf);

private:
  QStackedLayout* m_transportLayout{};

  UDPWidget* m_udp{};
  BasicTCPWidget* m_tcp{};
  WebsocketClientWidget* m_ws_client{};
};

class CoAPProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit CoAPProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  RateWidget* m_rate{};
  QComboBox* m_transport{};
  CoAPTransportWidget* m_transportWidget{};
  CoAPSpecificSettings m_settings;
};
}

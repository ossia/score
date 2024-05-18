#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/MQTT/MQTTSpecificSettings.hpp>

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

enum class MqttProtocol
{
  TCP = 0,
  WSClient = 1,
};

class MQTTTransportWidget : public QWidget
{
public:
  explicit MQTTTransportWidget(
      Device::ProtocolSettingsWidget& proto, QWidget* parent = nullptr);

  void setCurrentProtocol(MqttProtocol index);
  ossia::net::mqtt5_configuration configuration(MqttProtocol index) const noexcept;
  MqttProtocol setConfiguration(const ossia::net::mqtt5_configuration& conf);

private:
  QStackedLayout* m_transportLayout{};

  BasicTCPWidget* m_tcp{};
  WebsocketClientWidget* m_ws_client{};
};

class MQTTProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit MQTTProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  RateWidget* m_rate{};
  QComboBox* m_transport{};
  MQTTTransportWidget* m_transportWidget{};
  MQTTSpecificSettings m_settings;
};
}

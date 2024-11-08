// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MQTTProtocolSettingsWidget.hpp"

#include "MQTTProtocolFactory.hpp"
#include "MQTTSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/NetworkWidgets/TCPWidget.hpp>
#include <Protocols/NetworkWidgets/WebsocketClientWidget.hpp>
#include <Protocols/RateWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QStackedLayout>
#include <QVariant>

#include <wobjectimpl.h>

namespace Protocols
{

MQTTTransportWidget::MQTTTransportWidget(
    Device::ProtocolSettingsWidget& proto, QWidget* parent)
    : QWidget{parent}
{
  m_transportLayout = new QStackedLayout{this};
  m_transportLayout->setContentsMargins(0, 0, 0, 0);
  m_tcp = new BasicTCPWidget{proto, this};
  m_transportLayout->addWidget(m_tcp);

  m_ws_client = new WebsocketClientWidget{proto, this};
  m_transportLayout->addWidget(m_ws_client);
}

void MQTTTransportWidget::setCurrentProtocol(MqttProtocol index)
{
  m_transportLayout->setCurrentIndex((int)index);
}

MQTTProtocolSettingsWidget::MQTTProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  m_rate = new RateWidget{this};
  m_rate->setRate({});

  m_transport = new QComboBox{this};
  m_transport->addItems({"TCP", "Websocket Client"});
  checkForChanges(m_transport);

  m_transportWidget = new MQTTTransportWidget{*this, this};
  QObject::connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      [this](int idx) { m_transportWidget->setCurrentProtocol((MqttProtocol)idx); });

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate limit"), m_rate);
  layout->addRow(tr("Protocol"), m_transport);
  layout->addRow(m_transportWidget);
}

ossia::net::mqtt5_configuration
MQTTTransportWidget::configuration(MqttProtocol index) const noexcept
{
  ossia::net::mqtt5_configuration conf;
  switch(index)
  {
    case MqttProtocol::TCP:
      conf.transport = m_tcp->settings();
      break;
    case MqttProtocol::WSClient:
      conf.transport = m_ws_client->settings();
      break;
  }
  return conf;
}

MqttProtocol
MQTTTransportWidget::setConfiguration(const ossia::net::mqtt5_configuration& osc_conf)
{
  struct
  {
    MQTTTransportWidget& self;
    const ossia::net::mqtt5_configuration& osc_conf;
    MqttProtocol proto{};
    void operator()(const ossia::net::tcp_client_configuration& conf)
    {
      self.m_tcp->setSettings(conf);
      proto = MqttProtocol::TCP;
    }
    void operator()(const ossia::net::ws_client_configuration& conf)
    {
      self.m_ws_client->setSettings(conf);
      proto = MqttProtocol::WSClient;
    }
  } vis{*this, osc_conf};

  ossia::visit(vis, osc_conf.transport);
  return vis.proto;
}

Device::DeviceSettings MQTTProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = MQTTProtocolFactory::static_concreteKey();

  MQTTSpecificSettings osc = m_settings;
  osc.configuration
      = m_transportWidget->configuration((MqttProtocol)m_transport->currentIndex());
  osc.rate = m_rate->rate();
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

void MQTTProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if(settings.deviceSpecificSettings.canConvert<MQTTSpecificSettings>())
  {
    m_settings = settings.deviceSpecificSettings.value<MQTTSpecificSettings>();
    m_rate->setRate(m_settings.rate);
    auto proto = m_transportWidget->setConfiguration(m_settings.configuration);
    m_transport->setCurrentIndex((int)proto);
  }
}
}

// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CoAPProtocolSettingsWidget.hpp"

#include "CoAPProtocolFactory.hpp"
#include "CoAPSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/NetworkWidgets/TCPWidget.hpp>
#include <Protocols/NetworkWidgets/UDPWidget.hpp>
#include <Protocols/NetworkWidgets/WebsocketClientWidget.hpp>
#include <Protocols/RateWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QStackedLayout>
#include <QVariant>

#include <wobjectimpl.h>

namespace Protocols
{

CoAPTransportWidget::CoAPTransportWidget(
    Device::ProtocolSettingsWidget& proto, QWidget* parent)
    : QWidget{parent}
{
  m_transportLayout = new QStackedLayout{this};
  m_transportLayout->setContentsMargins(0, 0, 0, 0);

  m_udp = new UDPWidget{proto, this};
  m_transportLayout->addWidget(m_udp);

  m_tcp = new BasicTCPWidget{proto, this};
  m_transportLayout->addWidget(m_tcp);

  m_ws_client = new WebsocketClientWidget{proto, this};
  m_transportLayout->addWidget(m_ws_client);
}

void CoAPTransportWidget::setCurrentProtocol(CoapProtocol index)
{
  m_transportLayout->setCurrentIndex((int)index);
}

CoAPProtocolSettingsWidget::CoAPProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  m_rate = new RateWidget{this};
  m_rate->setRate({});

  m_transport = new QComboBox{this};
  m_transport->addItems({"UDP", "TCP", "Websocket Client"});
  checkForChanges(m_transport);

  m_transportWidget = new CoAPTransportWidget{*this, this};
  QObject::connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      [this](int idx) { m_transportWidget->setCurrentProtocol((CoapProtocol)idx); });

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate limit"), m_rate);
  layout->addRow(tr("Protocol"), m_transport);
  layout->addRow(m_transportWidget);
}

ossia::net::coap_client_configuration
CoAPTransportWidget::configuration(CoapProtocol index) const noexcept
{
  ossia::net::coap_client_configuration conf;
  switch(index)
  {
    case CoapProtocol::UDP:
      conf.transport = m_udp->settings();
      break;
    case CoapProtocol::TCP:
      conf.transport = m_tcp->settings();
      break;
    case CoapProtocol::WSClient:
      conf.transport = m_ws_client->settings();
      break;
  }
  return conf;
}

CoapProtocol
CoAPTransportWidget::setConfiguration(const ossia::net::coap_client_configuration& osc_conf)
{
  struct
  {
    CoAPTransportWidget& self;
    const ossia::net::coap_client_configuration& osc_conf;
    CoapProtocol proto{};
    void operator()(const ossia::net::udp_configuration& conf)
    {
      self.m_udp->setSettings(conf);
      proto = CoapProtocol::UDP;
    }
    void operator()(const ossia::net::tcp_configuration& conf)
    {
      self.m_tcp->setSettings(conf);
      proto = CoapProtocol::TCP;
    }
    void operator()(const ossia::net::ws_client_configuration& conf)
    {
      self.m_ws_client->setSettings(conf);
      proto = CoapProtocol::WSClient;
    }
  } vis{*this, osc_conf};

  ossia::visit(vis, osc_conf.transport);
  return vis.proto;
}

Device::DeviceSettings CoAPProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = CoAPProtocolFactory::static_concreteKey();

  CoAPSpecificSettings osc = m_settings;
  osc.configuration
      = m_transportWidget->configuration((CoapProtocol)m_transport->currentIndex());
  osc.rate = m_rate->rate();
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

void CoAPProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if(settings.deviceSpecificSettings.canConvert<CoAPSpecificSettings>())
  {
    m_settings = settings.deviceSpecificSettings.value<CoAPSpecificSettings>();
    m_rate->setRate(m_settings.rate);
    auto proto = m_transportWidget->setConfiguration(m_settings.configuration);
    m_transport->setCurrentIndex((int)proto);
  }
}
}

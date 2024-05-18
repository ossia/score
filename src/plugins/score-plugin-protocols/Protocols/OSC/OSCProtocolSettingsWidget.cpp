// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolSettingsWidget.hpp"

#include "OSCProtocolFactory.hpp"
#include "OSCSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Loading/JamomaDeviceLoader.hpp>
#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/NetworkWidgets/SerialWidget.hpp>
#include <Protocols/NetworkWidgets/TCPWidget.hpp>
#include <Protocols/NetworkWidgets/UDPWidget.hpp>
#include <Protocols/NetworkWidgets/UnixDatagramWidget.hpp>
#include <Protocols/NetworkWidgets/UnixStreamWidget.hpp>
#include <Protocols/NetworkWidgets/WebsocketClientWidget.hpp>
#include <Protocols/NetworkWidgets/WebsocketServerWidget.hpp>
#include <Protocols/RateWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QStackedLayout>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::RateWidget)
namespace Protocols
{

OSCTransportWidget::OSCTransportWidget(
    Device::ProtocolSettingsWidget& proto, QWidget* parent)
    : QWidget{parent}
{
  m_transportLayout = new QStackedLayout{this};
  m_transportLayout->setContentsMargins(0, 0, 0, 0);
  m_udp = new UDPWidget{proto, this};
  m_transportLayout->addWidget(m_udp);

  m_tcp = new TCPWidget{proto, this};
  m_transportLayout->addWidget(m_tcp);

  m_serial = new SerialWidget{proto, this};
  m_transportLayout->addWidget(m_serial);

  m_unix_dgram = new UnixDatagramWidget{proto, this};
  m_transportLayout->addWidget(m_unix_dgram);

  m_unix_stream = new UnixStreamWidget{proto, this};
  m_transportLayout->addWidget(m_unix_stream);

  m_ws_client = new WebsocketClientWidget{proto, this};
  m_transportLayout->addWidget(m_ws_client);

  m_ws_server = new WebsocketServerWidget{proto, this};
  m_transportLayout->addWidget(m_ws_server);
}

void OSCTransportWidget::setCurrentProtocol(OscProtocol index)
{
  m_transportLayout->setCurrentIndex((int)index);
}

OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  m_rate = new RateWidget{this};
  m_rate->setRate({});

  m_bonjour = new QCheckBox{this};
  m_bonjour->setWhatsThis(
      tr("If checked, the OSC device will expose itself over Bonjour with _osc._udp"));

  m_transport = new QComboBox{this};
  m_transport->addItems(
      {"UDP", "TCP", "Serial port", "Unix Datagram", "Unix Stream", "Websocket Client",
       "Websocket Server"});
  checkForChanges(m_transport);

  m_oscVersion = new QComboBox{this};
  m_oscVersion->addItems({"1.0", "1.1", "Extended"});

  m_transportWidget = new OSCTransportWidget{*this, this};
  QObject::connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      [this](int idx) { m_transportWidget->setCurrentProtocol((OscProtocol)idx); });

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate limit"), m_rate);
  layout->addRow(tr("Bonjour"), m_bonjour);
  layout->addRow(tr("OSC Version"), m_oscVersion);
  layout->addRow(tr("Protocol"), m_transport);
  layout->addRow(m_transportWidget);
}

ossia::net::osc_protocol_configuration
OSCTransportWidget::configuration(OscProtocol index) const noexcept
{
  ossia::net::osc_protocol_configuration conf;
  switch(index)
  {
    case OscProtocol::UDP:
      conf.transport = m_udp->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::TCP:
      conf.transport = m_tcp->settings();
      conf.framing = m_tcp->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::Serial:
      conf.transport = m_serial->settings();
      conf.framing = m_serial->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::UnixDatagram:
      conf.transport = m_unix_dgram->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::UnixStream:
      conf.transport = m_unix_stream->settings();
      conf.framing = m_unix_stream->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::WSClient:
      conf.transport = m_ws_client->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case OscProtocol::WSServer:
      conf.transport = m_ws_server->settings();
      conf.mode = ossia::net::osc_protocol_configuration::HOST;
      break;
  }
  return conf;
}

OscProtocol OSCTransportWidget::setConfiguration(
    const ossia::net::osc_protocol_configuration& osc_conf)
{
  struct
  {
    OSCTransportWidget& self;
    const ossia::net::osc_protocol_configuration& osc_conf;
    OscProtocol proto{};
    void operator()(const ossia::net::udp_configuration& conf)
    {
      self.m_udp->setSettings(conf);
      proto = OscProtocol::UDP;
    }
    void operator()(const ossia::net::tcp_configuration& conf)
    {
      self.m_tcp->setSettings(osc_conf, conf);
      proto = OscProtocol::TCP;
    }
    void operator()(const ossia::net::unix_dgram_configuration& conf)
    {
      self.m_unix_dgram->setSettings(conf);
      proto = OscProtocol::UnixDatagram;
    }
    void operator()(const ossia::net::unix_stream_configuration& conf)
    {
      self.m_unix_stream->setSettings(osc_conf, conf);
      proto = OscProtocol::UnixStream;
    }
    void operator()(const ossia::net::serial_configuration& conf)
    {
      self.m_serial->setSettings(osc_conf, conf);
      proto = OscProtocol::Serial;
    }
    void operator()(const ossia::net::ws_client_configuration& conf)
    {
      self.m_ws_client->setSettings(conf);
      proto = OscProtocol::WSClient;
    }
    void operator()(const ossia::net::ws_server_configuration& conf)
    {
      self.m_ws_server->setSettings(conf);
      proto = OscProtocol::WSServer;
    }
  } vis{*this, osc_conf};

  ossia::visit(vis, osc_conf.transport);
  return vis.proto;
}

Device::DeviceSettings OSCProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OSCProtocolFactory::static_concreteKey();

  OSCSpecificSettings osc = m_settings;

  using osc_version_t = decltype(ossia::net::osc_protocol_configuration::version);
  osc.configuration
      = m_transportWidget->configuration((OscProtocol)m_transport->currentIndex());
  osc.configuration.version = static_cast<osc_version_t>(m_oscVersion->currentIndex());
  osc.rate = m_rate->rate();
  osc.bonjour = m_bonjour->isChecked();
  osc.jsonToLoad.clear();

  // TODO list.append(m_namespaceFilePathEdit->text());
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

Device::Node OSCProtocolSettingsWidget::getDevice() const
{
  auto set = getSettings();
  Device::Node n{set, nullptr};

  const auto& json = m_settings.jsonToLoad;
  if(json.isEmpty())
    return n;

  // This is normal, we just check what we can instantiate.
  Device::loadDeviceFromScoreJSON(readJson(json), n);

  // Re-apply our original settings which may have been overwritten
  if(auto dev = n.target<Device::DeviceSettings>())
  {
    *dev = set;
  }

  return n;
}

void OSCProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if(settings.deviceSpecificSettings.canConvert<OSCSpecificSettings>())
  {
    m_settings = settings.deviceSpecificSettings.value<OSCSpecificSettings>();
    m_oscVersion->setCurrentIndex(m_settings.configuration.version);
    m_rate->setRate(m_settings.rate);
    m_bonjour->setChecked(m_settings.bonjour);
    auto proto = m_transportWidget->setConfiguration(m_settings.configuration);
    m_transport->setCurrentIndex((int)proto);
  }
}
}

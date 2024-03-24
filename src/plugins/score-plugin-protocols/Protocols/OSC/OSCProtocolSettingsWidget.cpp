// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolSettingsWidget.hpp"

#include "OSCProtocolFactory.hpp"
#include "OSCSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Loading/JamomaDeviceLoader.hpp>
#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/RateWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QStackedLayout>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::RateWidget)
namespace Protocols
{
using framing_type = decltype(ossia::net::osc_protocol_configuration::framing);
class UDPWidget : public QWidget
{
public:
  UDPWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    m_remotePort->setWhatsThis(
        tr("This is where the other software listens from incoming messages. Score will "
           "send packets to this port."));
    proto.checkForChanges(m_remotePort);

    m_localPort = new QSpinBox(this);
    m_localPort->setRange(0, 65535);
    m_localPort->setValue(9997);
    m_localPort->setWhatsThis(
        tr("This is where the other software sends feedback messages to. Score will "
           "listen for incoming OSC messages on this port."));
    proto.checkForChanges(m_localPort);

    m_broadcast = new QCheckBox{this};
    m_broadcast->setCheckState(Qt::Unchecked);
    m_broadcast->setWhatsThis(tr("Broadcast to every device in the IP broadcast range"));
    connect(m_broadcast, &QCheckBox::stateChanged, this, [this](int checked) {
      m_host->setEnabled(!checked);
    });

    m_host = new QLineEdit(this);
    m_host->setText("127.0.0.1");
    m_host->setWhatsThis(
        tr("This is the IP address of the computer the OSC-compatible software is "
           "located on. You can use 127.0.0.1 if the software runs on the same machine "
           "than score."));

    layout->addRow(tr("Device listening port"), m_remotePort);
    layout->addRow(tr("Broadcast"), m_broadcast);
    layout->addRow(tr("Device host"), m_host);
    layout->addRow(tr("score listening port"), m_localPort);
  }

  ossia::net::udp_configuration settings() const noexcept
  {
    ossia::net::udp_configuration conf;
    conf.local = ossia::net::receive_socket_configuration{
        "0.0.0.0", (uint16_t)m_localPort->value()};
    conf.remote = ossia::net::send_socket_configuration{
        m_host->text().toStdString(), (uint16_t)m_remotePort->value(),
        m_broadcast->isChecked()};
    return conf;
  }

  void setSettings(const ossia::net::udp_configuration& conf)
  {
    if(conf.remote)
    {
      m_remotePort->setValue(conf.remote->port);
      m_host->setText(QString::fromStdString(conf.remote->host));
      m_broadcast->setChecked(conf.remote->broadcast);
    }
    if(conf.local)
    {
      m_localPort->setValue(conf.local->port);
    }
  }

private:
  QSpinBox* m_localPort{};
  QSpinBox* m_remotePort{};
  QCheckBox* m_broadcast{};
  QLineEdit* m_host{};
};

class TCPWidget : public QWidget
{
public:
  TCPWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    m_remotePort->setWhatsThis(
        tr("This is the communication port used for the TCP connection."));

    m_host = new QLineEdit(this);
    m_host->setText("127.0.0.1");
    m_host->setWhatsThis(
        tr("This is the IP address of the computer the OSC-compatible software is "
           "located on. You can use 127.0.0.1 if the software runs on the same machine "
           "than score."));

    m_framing = new QComboBox{this};
    m_framing->addItems({"Size prefixing", "SLIP"});
    m_framing->setCurrentIndex(1);

    layout->addRow(tr("Port"), m_remotePort);
    layout->addRow(tr("Host"), m_host);
    layout->addRow(tr("Framing"), m_framing);
  }

  framing_type framing() const noexcept
  {
    return (framing_type)m_framing->currentIndex();
  }

  ossia::net::tcp_configuration settings() const noexcept
  {
    ossia::net::tcp_configuration conf;
    conf.port = m_remotePort->value();
    conf.host = m_host->text().toStdString();
    return conf;
  }

  void setSettings(
      const ossia::net::osc_protocol_configuration& c,
      const ossia::net::tcp_configuration& conf)
  {
    m_remotePort->setValue(conf.port);
    m_framing->setCurrentIndex(c.framing);
    m_host->setText(QString::fromStdString(conf.host));
  }

private:
  QSpinBox* m_remotePort{};
  QComboBox* m_framing{};
  QLineEdit* m_host{};
};

class UnixDatagramWidget : public QWidget
{
public:
  UnixDatagramWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QLineEdit(this);
    m_remotePort->setText("/tmp/ossia.a.socket");
    proto.checkForChanges(m_remotePort);

    m_localPort = new QLineEdit(this);
    m_localPort->setText("/tmp/ossia.b.socket");

    layout->addRow(tr("Input socket"), m_remotePort);
    layout->addRow(tr("Output socket"), m_localPort);
  }

  ossia::net::unix_dgram_configuration settings() const noexcept
  {
    ossia::net::unix_dgram_configuration conf;
    conf.local
        = ossia::net::receive_fd_configuration{m_remotePort->text().toStdString()};
    conf.remote = ossia::net::send_fd_configuration{m_localPort->text().toStdString()};
    return conf;
  }

  void setSettings(const ossia::net::unix_dgram_configuration& conf)
  {
    if(conf.remote)
    {
      m_localPort->setText(QString::fromStdString(conf.remote->fd));
    }
    if(conf.local)
    {
      m_remotePort->setText(QString::fromStdString(conf.local->fd));
    }
  }

private:
  QLineEdit* m_localPort{};
  QLineEdit* m_remotePort{};
};
class UnixStreamWidget : public QWidget
{
public:
  UnixStreamWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_host = new QLineEdit(this);
    m_host->setText("/tmp/ossia.socket");

    m_framing = new QComboBox{this};
    m_framing->addItems({"Size prefixing", "SLIP"});
    m_framing->setCurrentIndex(1);

    layout->addRow(tr("Path"), m_host);
    layout->addRow(tr("Framing"), m_framing);
  }

  framing_type framing() const noexcept
  {
    return (framing_type)m_framing->currentIndex();
  }

  ossia::net::unix_stream_configuration settings() const noexcept
  {
    ossia::net::unix_stream_configuration conf;
    conf.fd = m_host->text().toStdString();
    return conf;
  }

  void setSettings(
      const ossia::net::osc_protocol_configuration& c,
      const ossia::net::unix_stream_configuration& conf)
  {
    m_framing->setCurrentIndex(c.framing);
    m_host->setText(QString::fromStdString(conf.fd));
  }

private:
  QComboBox* m_framing{};
  QLineEdit* m_host{};
};
class WebsocketClientWidget : public QWidget
{
public:
  WebsocketClientWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_host = new QLineEdit(this);
    m_host->setText("ws://127.0.0.1:5567");

    layout->addRow(tr("Url"), m_host);
  }

  ossia::net::ws_client_configuration settings() const noexcept
  {
    ossia::net::ws_client_configuration conf;
    conf.url = m_host->text().toStdString();
    return conf;
  }

  void setSettings(const ossia::net::ws_client_configuration& conf)
  {
    m_host->setText(QString::fromStdString(conf.url));
  }

private:
  QLineEdit* m_host{};
};

class WebsocketServerWidget : public QWidget
{
public:
  WebsocketServerWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    proto.checkForChanges(m_remotePort);

    layout->addRow(tr("Port"), m_remotePort);
  }

  ossia::net::ws_server_configuration settings() const noexcept
  {
    ossia::net::ws_server_configuration conf;
    conf.port = m_remotePort->value();
    return conf;
  }

  void setSettings(const ossia::net::ws_server_configuration& conf)
  {
    m_remotePort->setValue(conf.port);
  }

private:
  QSpinBox* m_remotePort{};
};
class SerialWidget : public QWidget
{
public:
  SerialWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_baudRate = new QSpinBox(this);
    m_baudRate->setRange(0, 115200);
    m_baudRate->setValue(9600);

    m_host = new QLineEdit(this);
    m_host->setText("/dev/ttyUSB0");

    m_framing = new QComboBox{this};
    m_framing->addItems({"Size prefixing", "SLIP"});
    m_framing->setCurrentIndex(1);

    layout->addRow(tr("Host"), m_host);
    layout->addRow(tr("Baud rate"), m_baudRate);
    layout->addRow(tr("Framing"), m_framing);
  }

  framing_type framing() const noexcept
  {
    return (framing_type)m_framing->currentIndex();
  }

  ossia::net::serial_configuration settings() const noexcept
  {
    ossia::net::serial_configuration conf;
    conf.port = m_host->text().toStdString();
    conf.baud_rate = m_baudRate->value();
    return conf;
  }

  void setSettings(
      const ossia::net::osc_protocol_configuration& c,
      const ossia::net::serial_configuration& conf)
  {
    m_framing->setCurrentIndex(c.framing);
    m_host->setText(QString::fromStdString(conf.port));
    m_baudRate->setValue(conf.baud_rate);
  }

private:
  QLineEdit* m_host{};
  QSpinBox* m_baudRate{};
  QComboBox* m_framing{};
};

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
  m_transportLayout->setCurrentIndex(index);
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
    case UDP:
      conf.transport = m_udp->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case TCP:
      conf.transport = m_tcp->settings();
      conf.framing = m_tcp->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case Serial:
      conf.transport = m_serial->settings();
      conf.framing = m_serial->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case UnixDatagram:
      conf.transport = m_unix_dgram->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case UnixStream:
      conf.transport = m_unix_stream->settings();
      conf.framing = m_unix_stream->framing();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case WSClient:
      conf.transport = m_ws_client->settings();
      conf.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case WSServer:
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
      proto = UDP;
    }
    void operator()(const ossia::net::tcp_configuration& conf)
    {
      self.m_tcp->setSettings(osc_conf, conf);
      proto = TCP;
    }
    void operator()(const ossia::net::unix_dgram_configuration& conf)
    {
      self.m_unix_dgram->setSettings(conf);
      proto = UnixDatagram;
    }
    void operator()(const ossia::net::unix_stream_configuration& conf)
    {
      self.m_unix_stream->setSettings(osc_conf, conf);
      proto = UnixStream;
    }
    void operator()(const ossia::net::serial_configuration& conf)
    {
      self.m_serial->setSettings(osc_conf, conf);
      proto = Serial;
    }
    void operator()(const ossia::net::ws_client_configuration& conf)
    {
      self.m_ws_client->setSettings(conf);
      proto = WSClient;
    }
    void operator()(const ossia::net::ws_server_configuration& conf)
    {
      self.m_ws_server->setSettings(conf);
      proto = WSServer;
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
    m_transport->setCurrentIndex(proto);
  }
}
}

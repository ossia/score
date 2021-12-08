// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolSettingsWidget.hpp"

#include "OSCProtocolFactory.hpp"
#include "OSCSpecificSettings.hpp"

#include <Device/Loading/JamomaDeviceLoader.hpp>
#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/RateWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

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
  UDPWidget(OSCProtocolSettingsWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    parent->checkForChanges(m_remotePort);

    m_localPort = new QSpinBox(this);
    m_localPort->setRange(0, 65535);
    m_localPort->setValue(9997);
    parent->checkForChanges(m_localPort);

    m_host = new QLineEdit(this);
    m_host->setText("127.0.0.1");

    layout->addRow(tr("Device listening port"), m_remotePort);
    layout->addRow(tr("score listening port"), m_localPort);
    layout->addRow(tr("Host"), m_host);
  }

  ossia::net::udp_configuration settings() const noexcept
  {
    ossia::net::udp_configuration conf;
    conf.local = ossia::net::receive_socket_configuration{
        "0.0.0.0", (uint16_t)m_localPort->value()};
    conf.remote = ossia::net::send_socket_configuration{
        m_host->text().toStdString(), (uint16_t)m_remotePort->value()};
    return conf;
  }

  void setSettings(const ossia::net::udp_configuration& conf)
  {
    if (conf.remote)
    {
      m_remotePort->setValue(conf.remote->port);
      m_host->setText(QString::fromStdString(conf.remote->host));
    }
    if (conf.local)
    {
      m_localPort->setValue(conf.local->port);
    }
  }

private:
  QSpinBox* m_localPort{};
  QSpinBox* m_remotePort{};
  QLineEdit* m_host{};
};

class TCPWidget : public QWidget
{
public:
  TCPWidget(OSCProtocolSettingsWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);

    m_host = new QLineEdit(this);
    m_host->setText("127.0.0.1");

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

  void setSettings(const ossia::net::osc_protocol_configuration& c, const ossia::net::tcp_configuration& conf)
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
  UnixDatagramWidget(OSCProtocolSettingsWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QLineEdit(this);
    m_remotePort->setText("/tmp/ossia.a.socket");
    parent->checkForChanges(m_remotePort);

    m_localPort = new QLineEdit(this);
    m_localPort->setText("/tmp/ossia.b.socket");

    layout->addRow(tr("Input socket"), m_remotePort);
    layout->addRow(tr("Output socket"), m_localPort);
  }

  ossia::net::unix_dgram_configuration settings() const noexcept
  {
    ossia::net::unix_dgram_configuration conf;
    conf.local = ossia::net::receive_fd_configuration{
        m_remotePort->text().toStdString()};
    conf.remote
        = ossia::net::send_fd_configuration{m_localPort->text().toStdString()};
    return conf;
  }

  void setSettings(const ossia::net::unix_dgram_configuration& conf)
  {
    if (conf.remote)
    {
      m_localPort->setText(QString::fromStdString(conf.remote->fd));
    }
    if (conf.local)
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
  UnixStreamWidget(OSCProtocolSettingsWidget* parent)
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

  void setSettings(const ossia::net::osc_protocol_configuration& c, const ossia::net::unix_stream_configuration& conf)
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
  WebsocketClientWidget(OSCProtocolSettingsWidget* parent)
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
  WebsocketServerWidget(OSCProtocolSettingsWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    parent->checkForChanges(m_remotePort);

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
  SerialWidget(OSCProtocolSettingsWidget* parent)
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

  void setSettings(const ossia::net::osc_protocol_configuration& c, const ossia::net::serial_configuration& conf)
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

enum OscProtocols
{
  UDP = 0,
  TCP = 1,
  Serial = 2,
  UnixDatagram = 3,
  UnixStream = 4,
  WSClient = 5,
  WSServer = 6
};

OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  m_rate = new RateWidget{this};
  m_rate->setRate({});

  m_transport = new QComboBox{this};
  m_transport->addItems(
      {"UDP",
       "TCP",
       "Serial port",
       "Unix Datagram",
       "Unix Stream",
       "Websocket Client",
       "Websocket Server"});
  checkForChanges(m_transport);

  m_oscVersion = new QComboBox{this};
  m_oscVersion->addItems({"1.0", "1.1", "Extended"});
  m_transportLayout = new QStackedLayout{};
  m_transportLayout->setContentsMargins(0, 0, 0, 0);
  m_udp = new UDPWidget{this};
  m_transportLayout->addWidget(m_udp);

  m_tcp = new TCPWidget{this};
  m_transportLayout->addWidget(m_tcp);

  m_serial = new SerialWidget{this};
  m_transportLayout->addWidget(m_serial);

  m_unix_dgram = new UnixDatagramWidget{this};
  m_transportLayout->addWidget(m_unix_dgram);

  m_unix_stream = new UnixStreamWidget{this};
  m_transportLayout->addWidget(m_unix_stream);

  m_ws_client = new WebsocketClientWidget{this};
  m_transportLayout->addWidget(m_ws_client);

  m_ws_server = new WebsocketServerWidget{this};
  m_transportLayout->addWidget(m_ws_server);

  connect(
      m_transport,
      qOverload<int>(&QComboBox::currentIndexChanged),
      this,
      [=](int idx) { m_transportLayout->setCurrentIndex(idx); });

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("OSC Version"), m_oscVersion);
  layout->addRow(tr("Rate limit"), m_rate);
  layout->addRow(tr("Protocol"), m_transport);
  layout->addRow(m_transportLayout);
}

Device::DeviceSettings OSCProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OSCProtocolFactory::static_concreteKey();

  OSCSpecificSettings osc = m_settings;
  switch ((OscProtocols)m_transport->currentIndex())
  {
    case UDP:
      osc.configuration.transport = m_udp->settings();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case TCP:
      osc.configuration.transport = m_tcp->settings();
      osc.configuration.framing = m_tcp->framing();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case Serial:
      osc.configuration.transport = m_serial->settings();
      osc.configuration.framing = m_serial->framing();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case UnixDatagram:
      osc.configuration.transport = m_unix_dgram->settings();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case UnixStream:
      osc.configuration.transport = m_unix_stream->settings();
      osc.configuration.framing = m_unix_stream->framing();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case WSClient:
      osc.configuration.transport = m_ws_client->settings();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
      break;
    case WSServer:
      osc.configuration.transport = m_ws_server->settings();
      osc.configuration.mode = ossia::net::osc_protocol_configuration::HOST;
      break;
  }

  osc.rate = m_rate->rate();
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
  if (json.isEmpty())
    return n;

  // This is normal, we just check what we can instantiate.
  Device::loadDeviceFromScoreJSON(readJson(json), n);

  // Re-apply our original settings which may have been overwritten
  if (auto dev = n.target<Device::DeviceSettings>())
  {
    *dev = set;
  }

  return n;
}

void OSCProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if (settings.deviceSpecificSettings.canConvert<OSCSpecificSettings>())
  {
    m_settings = settings.deviceSpecificSettings.value<OSCSpecificSettings>();
    m_oscVersion->setCurrentIndex(m_settings.configuration.version);
    m_rate->setRate(m_settings.rate);
    struct vis
    {
      OSCProtocolSettingsWidget& self;
      void operator()(const ossia::net::udp_configuration& conf) const
      {
        self.m_udp->setSettings(conf);
        self.m_transport->setCurrentIndex(UDP);
      }
      void operator()(const ossia::net::tcp_configuration& conf) const
      {
        self.m_tcp->setSettings(self.m_settings.configuration, conf);
        self.m_transport->setCurrentIndex(TCP);
      }
      void operator()(const ossia::net::unix_dgram_configuration& conf) const
      {
        self.m_unix_dgram->setSettings(conf);
        self.m_transport->setCurrentIndex(UnixDatagram);
      }
      void operator()(const ossia::net::unix_stream_configuration& conf) const
      {
        self.m_unix_stream->setSettings(self.m_settings.configuration, conf);
        self.m_transport->setCurrentIndex(UnixStream);
      }
      void operator()(const ossia::net::serial_configuration& conf) const
      {
        self.m_serial->setSettings(self.m_settings.configuration, conf);
        self.m_transport->setCurrentIndex(Serial);
      }
      void operator()(const ossia::net::ws_client_configuration& conf) const
      {
        self.m_ws_client->setSettings(conf);
        self.m_transport->setCurrentIndex(WSClient);
      }
      void operator()(const ossia::net::ws_server_configuration& conf) const
      {
        self.m_ws_server->setSettings(conf);
        self.m_transport->setCurrentIndex(WSServer);
      }
    };

    using namespace std;
    using namespace boost::variant2;
    visit(vis{*this}, m_settings.configuration.transport);
  }
}
}

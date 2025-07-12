#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/NetworkWidgets/TCPWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
namespace Protocols
{

class TCPServerWidget : public QWidget
{
public:
  TCPServerWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
      : QWidget{parent}
  {
    auto layout = new QFormLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_remotePort = new QSpinBox(this);
    m_remotePort->setRange(0, 65535);
    m_remotePort->setValue(9996);
    proto.checkForChanges(m_remotePort);

    m_framing = new QComboBox{this};
    m_framing->addItems({"Size prefixing", "SLIP"});
    m_framing->setCurrentIndex(1);

    layout->addRow(tr("Port"), m_remotePort);
    layout->addRow(tr("Framing"), m_framing);
  }

  framing_type framing() const noexcept
  {
    return (framing_type)m_framing->currentIndex();
  }

  ossia::net::tcp_server_configuration settings() const noexcept
  {
    ossia::net::tcp_server_configuration conf;
    conf.bind = "0.0.0.0";
    conf.port = m_remotePort->value();
    return conf;
  }

  void setSettings(
      const ossia::net::osc_protocol_configuration& c,
      const ossia::net::tcp_server_configuration& conf)
  {
    m_remotePort->setValue(conf.port);
    m_framing->setCurrentIndex(c.framing);
  }

private:
  QSpinBox* m_remotePort{};
  QComboBox* m_framing{};
};
}

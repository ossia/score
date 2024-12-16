#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/widgets/HelpInteraction.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
namespace Protocols
{

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
    score::setHelp(m_remotePort, 
        tr("This is where the other software listens from incoming messages. Score will "
           "send packets to this port."));
    proto.checkForChanges(m_remotePort);

    m_localPort = new QSpinBox(this);
    m_localPort->setRange(0, 65535);
    m_localPort->setValue(9997);
    score::setHelp(m_localPort, 
        tr("This is where the other software sends feedback messages to. Score will "
           "listen for incoming OSC messages on this port."));
    proto.checkForChanges(m_localPort);

    m_broadcast = new QCheckBox{this};
    m_broadcast->setCheckState(Qt::Unchecked);
    score::setHelp(m_broadcast, tr("Broadcast to every device in the IP broadcast range"));

    m_host = new QLineEdit(this);
    m_host->setText("127.0.0.1");
    score::setHelp(m_host, 
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
    conf.local = ossia::net::inbound_socket_configuration{
        "0.0.0.0", (uint16_t)m_localPort->value()};
    conf.remote = ossia::net::outbound_socket_configuration{
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

}

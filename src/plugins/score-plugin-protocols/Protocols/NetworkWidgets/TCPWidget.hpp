#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>
#include <ossia/protocols/osc/osc_factory.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
namespace Protocols
{

using framing_type = decltype(ossia::net::osc_protocol_configuration::framing);
class BasicTCPWidget : public QWidget
{
public:
  BasicTCPWidget(Device::ProtocolSettingsWidget& proto, QWidget* parent)
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

    layout->addRow(tr("Port"), m_remotePort);
    layout->addRow(tr("Host"), m_host);
  }

  ossia::net::tcp_configuration settings() const noexcept
  {
    ossia::net::tcp_configuration conf;
    conf.port = m_remotePort->value();
    conf.host = m_host->text().toStdString();
    return conf;
  }

  void setSettings(const ossia::net::tcp_configuration& conf)
  {
    m_remotePort->setValue(conf.port);
    m_host->setText(QString::fromStdString(conf.host));
  }

private:
  QSpinBox* m_remotePort{};
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
}

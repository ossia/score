#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QFormLayout>
#include <QLineEdit>
namespace Protocols
{

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
}

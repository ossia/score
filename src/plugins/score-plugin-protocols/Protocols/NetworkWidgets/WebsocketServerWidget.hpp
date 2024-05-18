#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QFormLayout>
#include <QSpinBox>
namespace Protocols
{

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
}

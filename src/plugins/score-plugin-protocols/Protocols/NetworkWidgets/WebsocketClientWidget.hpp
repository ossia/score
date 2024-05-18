#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>

#include <QFormLayout>
#include <QLineEdit>
namespace Protocols
{

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
}

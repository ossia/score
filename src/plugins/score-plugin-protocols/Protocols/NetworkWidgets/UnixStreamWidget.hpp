#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/sockets/configuration.hpp>
#include <ossia/protocols/osc/osc_factory.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
namespace Protocols
{

using framing_type = decltype(ossia::net::osc_protocol_configuration::framing);
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
}

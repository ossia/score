#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/protocols/osc/osc_factory.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
namespace Protocols
{

using framing_type = decltype(ossia::net::osc_protocol_configuration::framing);
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

}

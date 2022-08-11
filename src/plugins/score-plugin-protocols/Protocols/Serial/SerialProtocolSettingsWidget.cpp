// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include "SerialProtocolFactory.hpp"
#include "SerialProtocolSettingsWidget.hpp"
#include "SerialSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Process/Script/ScriptWidget.hpp>

#include <score/widgets/ComboBox.hpp>
#include <score/widgets/Layout.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCodeEditor>
#include <QLineEdit>
#include <QVariant>
namespace Protocols
{
SerialProtocolSettingsWidget::SerialProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  QLabel* deviceNameLabel = new TextLabel(tr("Name"), this);
  m_name = new QLineEdit{this};

  QLabel* portLabel = new TextLabel(tr("Port"), this);
  m_port = new score::ComboBox{this};

  QLabel* rateLabel = new TextLabel(tr("Baud rate"), this);
  m_rate = new score::ComboBox{this};
  m_rate->setEditable(true);

  m_codeEdit = Process::createScriptWidget("JS");

  for(auto port : QSerialPortInfo::availablePorts())
    m_port->addItem(port.portName());

  for(auto rate : QSerialPortInfo::standardBaudRates())
    m_rate->addItem(QString::number(rate));

  QGridLayout* gLayout = new QGridLayout;

  gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_name, 0, 1, 1, 1);

  gLayout->addWidget(portLabel, 1, 0, 1, 1);
  gLayout->addWidget(m_port, 1, 1, 1, 1);

  gLayout->addWidget(rateLabel, 2, 0, 1, 1);
  gLayout->addWidget(m_rate, 2, 1, 1, 1);

  gLayout->addWidget(m_codeEdit, 3, 0, 1, 2);

  setLayout(gLayout);

  setDefaults();
}

void SerialProtocolSettingsWidget::setDefaults()
{
  SCORE_ASSERT(m_codeEdit);

  m_name->setText("newDevice");
  m_codeEdit->setPlainText("");
  m_port->setCurrentIndex(0);
  m_rate->setCurrentText("9600");
}

Device::DeviceSettings SerialProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_name->text();
  s.protocol = SerialProtocolFactory::static_concreteKey();

  SerialSpecificSettings specific;
  for(auto port : QSerialPortInfo::availablePorts())
    if(port.portName() == m_port->currentText())
      specific.port = port;

  for(auto rate : QSerialPortInfo::standardBaudRates())
    if(rate == m_rate->currentText().toInt())
    {
      specific.rate = rate;
      break;
    }

  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void SerialProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  SerialSpecificSettings specific;
  if(settings.deviceSpecificSettings.canConvert<SerialSpecificSettings>())
  {
    specific = settings.deviceSpecificSettings.value<SerialSpecificSettings>();

    int32_t rate{specific.rate};
    if(rate == 0)
      rate = 9600;

    m_port->setCurrentText(specific.port.portName());
    m_rate->setCurrentText(QString::number(rate));
    m_codeEdit->setPlainText(specific.text);
  }
}
}
#endif

// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolSettingsWidget.hpp"

#include "OSCProtocolFactory.hpp"
#include "OSCSpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/RateWidget.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::RateWidget)
namespace Protocols
{
OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_portInputSBox = new QSpinBox(this);
  m_portInputSBox->setRange(0, 65535);

  m_portOutputSBox = new QSpinBox(this);
  m_portOutputSBox->setRange(0, 65535);

  m_localHostEdit = new QLineEdit(this);

  m_rate = new RateWidget{this};

  auto layout = new QFormLayout{this};
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Device listening port"), m_portInputSBox);
  layout->addRow(tr("score listening port"), m_portOutputSBox);
  layout->addRow(tr("Host"), m_localHostEdit);
  layout->addRow(tr("Rate"), m_rate);
  setDefaults();
}

void OSCProtocolSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("OSCdevice");
  m_portOutputSBox->setValue(9997);
  m_portInputSBox->setValue(9996);
  m_localHostEdit->setText("127.0.0.1");
  m_rate->setRate({});
}

Device::DeviceSettings OSCProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OSCProtocolFactory::static_concreteKey();

  OSCSpecificSettings osc;
  osc.host = m_localHostEdit->text();
  osc.inputPort = m_portInputSBox->value();
  osc.outputPort = m_portOutputSBox->value();
  osc.rate = m_rate->rate();

  // TODO list.append(m_namespaceFilePathEdit->text());
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

void OSCProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  OSCSpecificSettings osc;
  if (settings.deviceSpecificSettings.canConvert<OSCSpecificSettings>())
  {
    osc = settings.deviceSpecificSettings.value<OSCSpecificSettings>();
    m_portInputSBox->setValue(osc.inputPort);
    m_portOutputSBox->setValue(osc.outputPort);
    m_localHostEdit->setText(osc.host);
    m_rate->setRate(osc.rate);
  }
}
}

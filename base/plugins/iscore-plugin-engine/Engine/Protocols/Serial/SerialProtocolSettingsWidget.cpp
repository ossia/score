#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QSpinBox>
#include <QString>
#include <QVariant>

#include "SerialProtocolSettingsWidget.hpp"
#include "SerialSpecificSettings.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <QFormLayout>
#include <QComboBox>
#include <QPlainTextEdit>
#include <iscore/widgets/JS/JSEdit.hpp>
class QWidget;

namespace Engine
{
namespace Network
{
SerialProtocolSettingsWidget::SerialProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new QLineEdit;
  m_port = new QComboBox;

  m_codeEdit = new JSEdit(this);
  m_codeEdit->setSizePolicy(
      QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_codeEdit->setMinimumHeight(300);

  for(auto port : QSerialPortInfo::availablePorts())
  {
      m_port->addItem(port.portName());
  }

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(tr("Port"), m_port);
  lay->addRow(tr("Code"), m_codeEdit);

  setLayout(lay);

  setDefaults();
}

void SerialProtocolSettingsWidget::setDefaults()
{
  ISCORE_ASSERT(m_codeEdit);

  m_name->setText("newDevice");
  m_codeEdit->setPlainText("");
  m_port->setCurrentIndex(0);
}

Device::DeviceSettings SerialProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_name->text();

  Network::SerialSpecificSettings specific;
  for(auto port : QSerialPortInfo::availablePorts())
  {
      if(port.portName() == m_port->currentText())
      {
          specific.port = port;
      }
  }

  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void SerialProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  Network::SerialSpecificSettings specific;
  if (settings.deviceSpecificSettings
          .canConvert<Network::SerialSpecificSettings>())
  {
    specific
        = settings.deviceSpecificSettings.value<Network::SerialSpecificSettings>();

    m_port->setCurrentText(specific.port.portName());
    m_codeEdit->setPlainText(specific.text);
  }
}
}
}

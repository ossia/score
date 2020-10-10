// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolSettingsWidget.hpp"

#include "MIDISpecificSettings.hpp"
#include "MIDIProtocolFactory.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <score/widgets/ComboBox.hpp>
#include <ossia-qt/name_utils.hpp>

#include <QCheckBox>
#include <QDebug>
#include <QRadioButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QString>
#include <QVariant>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::MIDIInputSettingsWidget)

namespace Protocols
{
MIDIInputSettingsWidget::MIDIInputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new QLineEdit{"MidiIn"};
  m_createWhole = new QCheckBox{tr("Create whole tree"), this};

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(m_createWhole);

  setLayout(lay);
  m_createWhole->setChecked(false);
  m_createWhole->setEnabled(true);
}

Device::DeviceSettings MIDIInputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MIDISpecificSettings midi = s.deviceSpecificSettings.value<MIDISpecificSettings>();
  s.name = m_name->text();
  s.protocol = MIDIInputProtocolFactory::static_concreteKey();
  midi.createWholeTree = m_createWhole->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MIDIInputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;

  // Clean up the name a bit
  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.split(':').front();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_name->setText(prettyName);
}
}


W_OBJECT_IMPL(Protocols::MIDIOutputSettingsWidget)

namespace Protocols
{
MIDIOutputSettingsWidget::MIDIOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new QLineEdit{"MidiOut"};
  m_createWhole = new QCheckBox{tr("Create whole tree"), this};

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(m_createWhole);

  setLayout(lay);
  m_createWhole->setChecked(false);
  m_createWhole->setEnabled(true);
}

Device::DeviceSettings MIDIOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MIDISpecificSettings midi = s.deviceSpecificSettings.value<MIDISpecificSettings>();
  s.name = m_name->text();
  s.protocol = MIDIOutputProtocolFactory::static_concreteKey();
  midi.createWholeTree = m_createWhole->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MIDIOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;

  // Clean up the name a bit
  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.split(':').front();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_name->setText(prettyName);

  if (settings.deviceSpecificSettings.canConvert<MIDISpecificSettings>())
  {
    m_createWhole->setChecked(settings.deviceSpecificSettings.value<MIDISpecificSettings>().createWholeTree);
  }
}

}

// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolSettingsWidget.hpp"

#include "MIDIProtocolFactory.hpp"
#include "MIDISpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/widgets/ComboBox.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QString>
#include <QVariant>

#include <libremidi/types.hpp>

#include <algorithm>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::MIDIInputSettingsWidget)

namespace Protocols
{
MIDIInputSettingsWidget::MIDIInputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_name);
  m_createWhole = new QCheckBox{tr("Create whole tree"), this};
  m_virtualPort = new QCheckBox{tr("Virtual Port"), this};
  m_velocityZeroIsNoteOff = new QCheckBox{tr("Velocity = 0 -> Note Off"), this};
  checkForChanges(m_virtualPort);

  m_controllers = new QComboBox{this};
  m_controllers->addItem(
      tr("Forward as received"),
      QVariant::fromValue(static_cast<int>(libremidi::controller_translation::raw)));
  m_controllers->setItemData(
      0,
      tr("Every control change reaches the score exactly as the device sent it.\n"
         "Choose this for synths and controllers that use bank select, data entry\n"
         "or the RPN and NRPN controller numbers for their own purposes."),
      Qt::ToolTipRole);
  m_controllers->addItem(
      tr("Combine RPN, NRPN and bank select"),
      QVariant::fromValue(static_cast<int>(libremidi::controller_translation::semantic)));
  m_controllers->setItemData(
      1,
      tr("A data entry on a selected RPN or NRPN reaches the score as one parameter\n"
         "message, and a bank select travels with the program change that follows it.\n"
         "Controllers 0, 6, 32, 38 and 98 to 101 are not forwarded on their own."),
      Qt::ToolTipRole);
  m_controllers->setToolTip(
      tr("What the bank select, data entry, RPN and NRPN controllers of this input become.\n"
         "Forward as received: each one stays a control change, as the device sent it.\n"
         "Combine: they arrive as RPN, NRPN and program change with bank messages."));
  checkForChanges(m_controllers);

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(m_createWhole);
  lay->addRow(m_virtualPort);
  lay->addRow(m_velocityZeroIsNoteOff);
  lay->addRow(tr("Controllers"), m_controllers);

  setLayout(lay);
  m_createWhole->setChecked(false);
  m_createWhole->setEnabled(false);
  m_virtualPort->setChecked(false);
#if defined(_WIN32)
  m_virtualPort->setEnabled(false); //virtual port creation is unsupported by Windows
#else
  m_virtualPort->setEnabled(true);
#endif
}

Device::DeviceSettings MIDIInputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MIDISpecificSettings midi = s.deviceSpecificSettings.value<MIDISpecificSettings>();
  s.name = m_name->text();
  s.protocol = MIDIInputProtocolFactory::static_concreteKey();
  midi.createWholeTree = m_createWhole->isChecked();
  midi.virtualPort = m_virtualPort->isChecked();
  midi.velocityZeroIsNoteOff = m_velocityZeroIsNoteOff->isChecked();
  midi.controllers
      = static_cast<libremidi::controller_translation>(m_controllers->currentData().toInt());
  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MIDIInputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;
  const auto& s = m_current.deviceSpecificSettings.value<MIDISpecificSettings>();
  m_createWhole->setChecked(s.createWholeTree);
  m_virtualPort->setChecked(s.virtualPort);
  m_velocityZeroIsNoteOff->setChecked(s.velocityZeroIsNoteOff);
  m_controllers->setCurrentIndex(
      std::max(0, m_controllers->findData(static_cast<int>(s.controllers))));

  // Clean up the name a bit
  auto pretty_name = settings.name;
  if(!pretty_name.isEmpty())
  {
    pretty_name = pretty_name.split(':').front();
    ossia::net::sanitize_device_name(pretty_name);
  }
  m_name->setText(pretty_name);
}
}

W_OBJECT_IMPL(Protocols::MIDIOutputSettingsWidget)

namespace Protocols
{
MIDIOutputSettingsWidget::MIDIOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_name);
  m_createWhole = new QCheckBox{tr("Create whole tree"), this};
  m_virtualPort = new QCheckBox{tr("Virtual Port"), this};
  m_velocityZeroIsNoteOff = new QCheckBox{tr("Note Off -> Velocity = 0"), this};
  checkForChanges(m_virtualPort);

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(m_createWhole);
  lay->addRow(m_virtualPort);
  lay->addRow(m_velocityZeroIsNoteOff);

  setLayout(lay);
  m_createWhole->setChecked(false);
  m_createWhole->setEnabled(true);
  m_virtualPort->setChecked(false);

  // FIXME do this depending on the API instead!
#if defined(_WIN32)
  m_virtualPort->setEnabled(false); //virtual port creation is unsupported by Windows
#else
  m_virtualPort->setEnabled(true);
#endif
}

Device::DeviceSettings MIDIOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MIDISpecificSettings midi = s.deviceSpecificSettings.value<MIDISpecificSettings>();
  s.name = m_name->text();
  s.protocol = MIDIOutputProtocolFactory::static_concreteKey();
  midi.createWholeTree = m_createWhole->isChecked();
  midi.virtualPort = m_virtualPort->isChecked();
  midi.velocityZeroIsNoteOff = m_velocityZeroIsNoteOff->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MIDIOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;
  const auto& s = m_current.deviceSpecificSettings.value<MIDISpecificSettings>();
  m_createWhole->setChecked(s.createWholeTree);
  m_virtualPort->setChecked(s.virtualPort);
  m_velocityZeroIsNoteOff->setChecked(s.velocityZeroIsNoteOff);

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

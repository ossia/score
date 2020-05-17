// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MIDIProtocolSettingsWidget.hpp"

#include "MIDISpecificSettings.hpp"

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <score/widgets/ComboBox.hpp>

#include <QCheckBox>
#include <QDebug>
#include <QRadioButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QString>
#include <QVariant>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::MIDIProtocolSettingsWidget)
class QWidget;

namespace Protocols
{
MIDIProtocolSettingsWidget::MIDIProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new QLineEdit{"MidiDevice"};
  m_inButton = new QRadioButton{tr("Send"), this};
  m_inButton->setAutoExclusive(true);
  m_outButton = new QRadioButton{tr("Receive"), this};
  m_outButton->setAutoExclusive(true);
  m_deviceCBox = new score::ComboBox{this};
  m_createWhole = new QCheckBox{tr("Create whole tree"), this};

  auto gb_lay = new QHBoxLayout;
  gb_lay->setContentsMargins(0, 0, 0, 0);
  gb_lay->addWidget(m_inButton);
  gb_lay->addWidget(m_outButton);

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(tr("Type"), gb_lay);
  lay->addRow(tr("Device"), m_deviceCBox);
  lay->addRow(m_createWhole);

  setLayout(lay);

  this->setTabOrder(m_name, m_inButton);
  this->setTabOrder(m_inButton, m_deviceCBox);

  connect(m_inButton, &QAbstractButton::toggled, this, [this](bool b) {
    if (b)
    {
      updateDevices(ossia::net::midi::midi_info::Type::RemoteInput);
      m_createWhole->setChecked(true);
      m_createWhole->setEnabled(false);
    }
  });
  connect(m_outButton, &QAbstractButton::toggled, this, [this](bool b) {
    if (b)
    {
      updateDevices(ossia::net::midi::midi_info::Type::RemoteOutput);
      m_createWhole->setChecked(false);
      m_createWhole->setEnabled(true);
    }
  });

  m_inButton->setChecked(true);
  updateInputDevices();
}

Device::DeviceSettings MIDIProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  MIDISpecificSettings midi;
  s.name = m_name->text();

  midi.io = m_inButton->isChecked() ? MIDISpecificSettings::IO::In : MIDISpecificSettings::IO::Out;
  midi.endpoint = m_deviceCBox->currentText();
  midi.port = m_deviceCBox->currentData().toInt();
  midi.createWholeTree = m_createWhole->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MIDIProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  int index = m_deviceCBox->findText(settings.name);

  if (index >= 0 && index < m_deviceCBox->count())
  {
    m_deviceCBox->setCurrentIndex(index);
  }

  if (settings.deviceSpecificSettings.canConvert<MIDISpecificSettings>())
  {
    MIDISpecificSettings midi = settings.deviceSpecificSettings.value<MIDISpecificSettings>();
    if (midi.io == MIDISpecificSettings::IO::In)
    {
      m_inButton->setChecked(true);
    }
    else
    {
      m_outButton->setChecked(true);
    }

    m_createWhole->setChecked(midi.createWholeTree);
    m_deviceCBox->setCurrentText(midi.endpoint);
    // TODO <!> setData <!> (midi.port)
  }
}

void MIDIProtocolSettingsWidget::updateDevices(ossia::net::midi::midi_info::Type t)
{
  m_deviceCBox->clear();

  try
  {
    auto prot = std::make_unique<ossia::net::midi::midi_protocol>();
    auto vec = prot->scan();

    for (auto& elt : vec)
    {
      if (elt.type == t)
      {
        m_deviceCBox->addItem(QString::fromStdString(elt.device), QVariant::fromValue(elt.port));
      }
    }
    if (m_deviceCBox->count() > 0)
      m_deviceCBox->setCurrentIndex(0);
  }
  catch (std::exception& e)
  {
    qDebug() << e.what();
  }
}

void MIDIProtocolSettingsWidget::updateInputDevices()
{
  updateDevices(ossia::net::midi::midi_info::Type::RemoteInput);
}

void MIDIProtocolSettingsWidget::updateOutputDevices()
{
  updateDevices(ossia::net::midi::midi_info::Type::RemoteOutput);
}
}

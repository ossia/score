// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MCUProtocolSettingsWidget.hpp"

#include "MCUProtocolFactory.hpp"
#include "MCUSpecificSettings.hpp"

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

#include <libremidi/libremidi.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::MCUSettingsWidget)

namespace Protocols
{
MCUSettingsWidget::MCUSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_name = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_name);
  m_midiin = new QComboBox{this};
  m_midiout = new QComboBox{this};
  checkForChanges(m_midiin);
  checkForChanges(m_midiout);

  auto lay = new QFormLayout;
  lay->addRow(tr("Name"), m_name);
  lay->addRow(m_midiin);
  lay->addRow(m_midiout);

  libremidi::observer_configuration conf;
  conf.input_added = [this](const libremidi::input_port& ip) {
    QMetaObject::invokeMethod(this, [this, ip] {
      m_ins.push_back(ip);

      int idx = (int)m_ins.size() - 1;
      m_midiin->addItem(QString::fromStdString(ip.display_name), idx);
    });
  };
  conf.output_added = [this](const libremidi::output_port& ip) {
    QMetaObject::invokeMethod(this, [this, ip] {
      m_outs.push_back(ip);

      int idx = (int)m_outs.size() - 1;
      m_midiout->addItem(QString::fromStdString(ip.display_name), idx);
    });
  };
  conf.notify_in_constructor = true;
  conf.track_hardware = true;
  conf.track_virtual = true;
  m_observer = std::make_unique<libremidi::observer>(conf);
  setLayout(lay);
}

MCUSettingsWidget::~MCUSettingsWidget() { }
Device::DeviceSettings MCUSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_current;
  MCUSpecificSettings midi = s.deviceSpecificSettings.value<MCUSpecificSettings>();
  s.name = m_name->text();
  s.protocol = MCUProtocolFactory::static_concreteKey();

  const int in_idx = m_midiin->currentIndex();
  if(in_idx >= 0 && in_idx < std::ssize(m_ins))
    midi.input_handle = m_ins[in_idx];

  const int out_idx = m_midiout->currentIndex();
  if(out_idx >= 0 && out_idx < std::ssize(m_outs))
    midi.output_handle = m_outs[out_idx];

  s.protocol = MCUProtocolFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(midi);

  return s;
}

void MCUSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_current = settings;
  const auto& s = m_current.deviceSpecificSettings.value<MCUSpecificSettings>();

  // Clean up the name a bit
  auto pretty_name = settings.name;
  if(!pretty_name.isEmpty())
  {
    pretty_name = pretty_name.split(':').front();
    ossia::net::sanitize_device_name(pretty_name);
  }

  // FIXME
  m_name->setText(pretty_name);
}
}

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiProcessInspector.hpp"
#include <Midi/Commands/SetOutput.hpp>

#include <Engine/Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Engine/Protocols/MIDI/MIDISpecificSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>

namespace Midi
{

InspectorWidget::InspectorWidget(
    const ProcessModel& model,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{model, parent}
{
  auto vlay = new score::MarginLess<QFormLayout>{this};
  auto plug = doc.findPlugin<Explorer::DeviceDocumentPlugin>();

  m_devices = new QComboBox;
  vlay->addRow(tr("Midi out"), m_devices);
  for (auto& device : plug->rootNode())
  {
    Device::DeviceSettings set = device.get<Device::DeviceSettings>();
    if (set.protocol
        == Engine::Network::MIDIProtocolFactory::static_concreteKey())
    {
      m_devices->addItem(set.name);
    }
  }

  con(model, &ProcessModel::deviceChanged, this, [=](const QString& dev) {
    if (dev != m_devices->currentText())
    {
      m_devices->setCurrentText(dev);
    }
  });

  if (m_devices->findText(model.device()) != -1)
  {
    m_devices->setCurrentText(model.device());
  }
  else
  {
    m_devices->addItem(model.device());
    QFont f;
    f.setItalic(true);
    m_devices->setItemData(m_devices->count() - 1, f, Qt::FontRole);
    m_devices->setCurrentIndex(m_devices->count() - 1);
  }

  connect(
      m_devices, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [&](int idx) {
        CommandDispatcher<> d{doc.commandStack};
        d.submitCommand(new SetOutput{model, m_devices->itemText(idx)});
      });

  m_chan = new QSpinBox;
  m_chan->setMinimum(1);
  m_chan->setMaximum(16);

  vlay->addRow(tr("Channel"), m_chan);
  m_chan->setValue(model.channel());
  con(model, &ProcessModel::channelChanged, this, [=](int n) {
    if (m_chan->value() != n)
      m_chan->setValue(n);
  });
  connect(m_chan, SignalUtils::QSpinBox_valueChanged_int(), this, [&](int n) {
    if (model.channel() != n)
    {
      CommandDispatcher<> d{doc.commandStack};
      d.submitCommand(new SetChannel{model, n});
    }
  });
}
}

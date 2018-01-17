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

  ///// DEVICES /////
  {
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
  }

  ///// CHAN /////
  {
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

  ///// RANGE /////
  {
    auto [min, max] = model.range();
    m_min = new QSpinBox;
    m_max = new QSpinBox;
    m_min->setRange(0, 127);
    m_max->setRange(0, 127);

    m_min->setValue(min);
    m_max->setValue(max);

    vlay->addRow(tr("Min"), m_min);
    vlay->addRow(tr("Max"), m_max);

    con(model, &ProcessModel::rangeChanged,
        this, [=] (int min, int max) {
      m_min->setValue(min);
      m_max->setValue(max);
    });

    connect(m_min, &QSpinBox::editingFinished,
            this, [=,&model,&doc] {
      auto n = m_min->value();
      if (model.range().first != n)
      {
        CommandDispatcher<> d{doc.commandStack};
        d.submitCommand(new SetRange{model, n, m_max->value()});
      }
    });
    connect(m_max, &QSpinBox::editingFinished,
            this, [=,&model,&doc] {
      auto n = m_max->value();
      if (model.range().second != n)
      {
        CommandDispatcher<> d{doc.commandStack};
        d.submitCommand(new SetRange{model, m_min->value(), n});
      }
    });

  }
}
}

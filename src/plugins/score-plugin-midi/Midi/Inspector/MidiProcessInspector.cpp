// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiProcessInspector.hpp"

#include <Midi/Commands/SetOutput.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

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
        d.submit(new SetChannel{model, n});
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

    con(model, &ProcessModel::rangeChanged, this, [=](int min, int max) {
      m_min->setValue(min);
      m_max->setValue(max);
    });

    connect(m_min, &QSpinBox::editingFinished, this, [=, &model, &doc] {
      auto n = m_min->value();
      if (model.range().first != n)
      {
        CommandDispatcher<> d{doc.commandStack};
        d.submit(new SetRange{model, n, m_max->value()});
      }
    });
    connect(m_max, &QSpinBox::editingFinished, this, [=, &model, &doc] {
      auto n = m_max->value();
      if (model.range().second != n)
      {
        CommandDispatcher<> d{doc.commandStack};
        d.submit(new SetRange{model, m_min->value(), n});
      }
    });
  }
}
}

#pragma once
#include <Patternist/Commands/AddLane.hpp>
#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/tools/Bind.hpp>

#include <QFormLayout>
#include <QSpinBox>

namespace Patternist
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& obj,
      const score::DocumentContext& doc,
      QWidget* parent)
      : InspectorWidgetDelegate_T{obj, parent}
      , m_dispatcher{doc.commandStack}
      , m_channel{this}
      , m_currentPattern{this}
      , m_lanes{this}
      , m_duration{this}
      , m_rate{this}
  {
    m_duration.setRange(4, 32);
    m_rate.setRange(1, 64);
    m_channel.setRange(1, 16);
    m_lanes.setRange(1, 127);

    m_channel.setValue(obj.channel());

    const Pattern& pat = obj.patterns()[obj.currentPattern()];
    m_lanes.setValue(pat.lanes.size());
    m_duration.setValue(pat.length);
    m_rate.setValue(pat.division);

    auto lay = new QFormLayout{this};

    con(process(), &ProcessModel::channelChanged, this, [&] (int c) {
      if(c != m_channel.value())
        m_channel.setValue(c);
    });
    con(process(), &ProcessModel::currentPatternChanged, this, [&] (int c) {
      if(c == m_currentPattern.value())
        return;

      m_currentPattern.setValue(c);

      const Pattern& pat = obj.patterns()[c];
      m_lanes.setValue(pat.lanes.size());
      m_duration.setValue(pat.length);
      m_rate.setValue(pat.division);
    });
    con(process(), &ProcessModel::patternsChanged, this, [&] {
      const Pattern& pat = obj.patterns()[obj.currentPattern()];
      m_lanes.setValue(pat.lanes.size());
      m_duration.setValue(pat.length);
      m_rate.setValue(pat.division);
    });

    con(m_channel, &QSpinBox::editingFinished, this, [&]() {
      // m_dispatcher.submit<SetStepCount>(obj, m_channel.value());
      // m_dispatcher.commit();
    });
    con(m_lanes, &QSpinBox::editingFinished, this, [&]() {
      // m_dispatcher.submit<SetStepDuration>(obj, m_lanes.value());
      // m_dispatcher.commit();
    });
    con(m_currentPattern, &QSpinBox::editingFinished, this, [&]() {
      // m_dispatcher.submit<SetStepDuration>(obj, m_lanes.value());
      // m_dispatcher.commit();
    });
    con(m_duration, &QDoubleSpinBox::editingFinished, this, [&]() {
      // m_dispatcher.submit<SetMin>(obj, m_duration.value());
      // m_dispatcher.commit();
    });
    con(m_rate, &QDoubleSpinBox::editingFinished, this, [&]() {
      // m_dispatcher.submit<SetMax>(obj, m_rate.value());
      // m_dispatcher.commit();
    });

    lay->addRow(tr("Channel"), &m_channel);
    lay->addRow(tr("Current pattern"), &m_currentPattern);
    lay->addRow(tr("Lanes"), &m_lanes);
    lay->addRow(tr("Steps"), &m_duration);
    lay->addRow(tr("Rate"), &m_rate);
  }

private:
  OngoingCommandDispatcher m_dispatcher;

  QSpinBox m_channel;
  QSpinBox m_currentPattern;
  QSpinBox m_lanes;
  QDoubleSpinBox m_duration;
  QDoubleSpinBox m_rate;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("03d55730-fc4a-42a7-b573-35c330c5bad2")
};
}

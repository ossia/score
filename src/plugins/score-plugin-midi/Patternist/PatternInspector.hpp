#pragma once
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



    con(m_channel, qOverload<int>(&QSpinBox::valueChanged), this, [&]() {
      m_dispatcher.submit<SetPatternChannel>(obj, m_channel.value());
    });
    con(m_channel, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.commit();
    });


    con(m_lanes, qOverload<int>(&QSpinBox::valueChanged), this, [&]() {
      int n = m_lanes.value();
      if(n <= 0)
        return;

      auto p = obj.patterns()[obj.currentPattern()];
      if(n < p.lanes.size())
      {
        p.lanes.resize(n);
      }
      else
      {
        auto last_lane = p.lanes.back();
        while(p.lanes.size() < n)
          p.lanes.push_back(last_lane);
      }

      m_dispatcher.submit<UpdatePattern>(obj, obj.currentPattern(), p);
    });

    con(m_lanes, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.commit();
    });


    con(m_currentPattern, qOverload<int>(&QSpinBox::valueChanged), this, [&]() {
      m_dispatcher.submit<SetCurrentPattern>(obj, m_currentPattern.value());
    });
    con(m_currentPattern, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.commit();
    });


    con(m_duration, qOverload<int>(&QSpinBox::valueChanged), this, [&]() {
      int n = m_duration.value();
      if(n <= 0)
        return;

      auto p = obj.patterns()[obj.currentPattern()];
      p.length = n;
      if(p.length > p.lanes[0].pattern.size())
      {
        for(auto& lane : p.lanes)
        {
          lane.pattern.resize(n);
        }
      }

      m_dispatcher.submit<UpdatePattern>(obj, obj.currentPattern(), p);
    });
    con(m_duration, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.commit();
    });


    con(m_rate, &QDoubleSpinBox::editingFinished, this, [&]() {
      auto p = obj.patterns()[obj.currentPattern()];
      p.division = m_duration.value();
      m_dispatcher.submit<UpdatePattern>(obj, obj.currentPattern(), p);
      m_dispatcher.commit();
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
  QSpinBox m_duration;
  QDoubleSpinBox m_rate;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("03d55730-fc4a-42a7-b573-35c330c5bad2")
};
}

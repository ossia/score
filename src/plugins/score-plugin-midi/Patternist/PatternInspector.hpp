#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternModel.hpp>

namespace Patternist
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& obj, const score::DocumentContext& doc, QWidget* parent)
      : InspectorWidgetDelegate_T{obj, parent}
      , m_dispatcher{doc.commandStack}
      , m_channel{this}
      , m_lanes{this}
      , m_length{this}
      , m_rate{this}
      , m_labels{this}
  {
    m_channel.setRange(1, 16);
    m_lanes.setRange(1, 127);
    m_length.setRange(1, Pattern::maxLength);
    m_rate.setRange(1, 64);
    m_rate.setPrefix(QStringLiteral("1/"));
    m_rate.setToolTip(tr("Steps per whole note"));
    for(auto box : {&m_channel, &m_lanes, &m_length, &m_rate})
      box->setKeyboardTracking(false);

    m_labels.addItem(tr("Note and number"), int(LaneLabels::NameAndNumber));
    m_labels.addItem(tr("Note name"), int(LaneLabels::Name));
    m_labels.addItem(tr("MIDI number"), int(LaneLabels::Number));
    m_labels.addItem(tr("General MIDI drum"), int(LaneLabels::DrumName));

    auto lay = new QFormLayout{this};
    lay->addRow(tr("Channel"), &m_channel);
    lay->addRow(tr("Lanes"), &m_lanes);
    lay->addRow(tr("Steps"), &m_length);
    lay->addRow(tr("Rate"), &m_rate);
    lay->addRow(tr("Labels"), &m_labels);

    updateFromModel();

    con(obj, &ProcessModel::channelChanged, this, [this] { updateFromModel(); });
    con(obj, &ProcessModel::currentPatternChanged, this, [this] { updateFromModel(); });
    con(obj, &ProcessModel::patternsChanged, this, [this] { updateFromModel(); });
    con(obj, &ProcessModel::laneLabelsChanged, this, [this] { updateFromModel(); });

    con(m_channel, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
      if(v != process().channel())
        m_dispatcher.submit<SetPatternChannel>(process(), v);
    });

    con(m_lanes, qOverload<int>(&QSpinBox::valueChanged), this, [this](int n) {
      editPattern([n](Pattern& p) {
        if(n < int(p.lanes.size()))
          p.lanes.resize(n);
        while(int(p.lanes.size()) < n)
          addLane(p);
      });
    });

    con(m_length, qOverload<int>(&QSpinBox::valueChanged), this,
        [this](int n) { editPattern([n](Pattern& p) { setLength(p, n); }); });

    con(m_rate, qOverload<int>(&QSpinBox::valueChanged), this,
        [this](int n) { editPattern([n](Pattern& p) { p.division = n; }); });

    con(m_labels, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
      const auto l = LaneLabels(m_labels.currentData().toInt());
      if(l != process().laneLabels())
        m_dispatcher.submit<SetLaneLabels>(process(), l);
    });
  }

private:
  template <typename F>
  void editPattern(F f)
  {
    const auto& obj = process();
    if(!ossia::valid_index(obj.currentPattern(), obj.patterns()))
      return;
    auto p = obj.patterns()[obj.currentPattern()];
    f(p);
    normalize(p);
    if(p != obj.patterns()[obj.currentPattern()])
      m_dispatcher.submit<UpdatePattern>(obj, obj.currentPattern(), p);
  }

  void updateFromModel()
  {
    const auto& obj = process();
    QSignalBlocker b0{m_channel}, b1{m_lanes}, b2{m_length}, b3{m_rate}, b4{m_labels};
    m_channel.setValue(obj.channel());
    m_labels.setCurrentIndex(m_labels.findData(int(obj.laneLabels())));

    const bool valid = ossia::valid_index(obj.currentPattern(), obj.patterns());
    for(auto box : {&m_lanes, &m_length, &m_rate})
      box->setEnabled(valid);
    if(!valid)
      return;
    const Pattern& pat = obj.patterns()[obj.currentPattern()];
    m_lanes.setValue(pat.lanes.size());
    m_length.setValue(pat.length);
    m_rate.setValue(pat.division);
  }

  CommandDispatcher<> m_dispatcher;

  QSpinBox m_channel;
  QSpinBox m_lanes;
  QSpinBox m_length;
  QSpinBox m_rate;
  QComboBox m_labels;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("03d55730-fc4a-42a7-b573-35c330c5bad2")
};
}

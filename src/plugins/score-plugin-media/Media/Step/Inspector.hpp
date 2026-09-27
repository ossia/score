#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Media/Step/Commands.hpp>
#include <Media/Step/Model.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFormLayout>
#include <QSpinBox>

namespace Media
{
namespace Step
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Model>
{
public:
  explicit InspectorWidget(
      const Model& obj, const score::DocumentContext& doc, QWidget* parent)
      : InspectorWidgetDelegate_T{obj, parent}
      , m_dispatcher{doc.dispatcher}
      , m_count{this}
      , m_min{this}
      , m_max{this}
  {
    m_min.setRange(-100000, 100000);
    m_max.setRange(-100000, 100000);
    m_count.setRange(1, Model::maxSteps);
    m_count.setValue(obj.stepCount());
    m_min.setValue(obj.min());
    m_max.setValue(obj.max());

    auto lay = new QFormLayout{this};

    con(process(), &Model::stepCountChanged, this,
        [&] { m_count.setValue(obj.stepCount()); });
    con(process(), &Model::minChanged, this, [&] { m_min.setValue(obj.min()); });
    con(process(), &Model::maxChanged, this, [&] { m_max.setValue(obj.max()); });

    con(m_count, &QSpinBox::editingFinished, this, [&]() {
      m_dispatcher.submit<SetStepCount>(obj, m_count.value());
      m_dispatcher.commit();
    });
    con(m_min, &QDoubleSpinBox::editingFinished, this, [&]() {
      m_dispatcher.submit<SetMin>(obj, m_min.value());
      m_dispatcher.commit();
    });
    con(m_max, &QDoubleSpinBox::editingFinished, this, [&]() {
      m_dispatcher.submit<SetMax>(obj, m_max.value());
      m_dispatcher.commit();
    });

    // Of the sequence shown; the others keep theirs.
    con(process(), &Model::currentSequenceChanged, this,
        [&] { m_count.setValue(obj.stepCount()); });
    lay->addRow(tr("Count"), &m_count);
    lay->addRow(tr("Min"), &m_min);
    lay->addRow(tr("Max"), &m_max);
  }

private:
  OngoingCommandDispatcher& m_dispatcher;

  QSpinBox m_count;
  QDoubleSpinBox m_min;
  QDoubleSpinBox m_max;
};
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("a7c0ecb5-70a1-476c-8187-075076e413d2")
};
}
}

#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <Media/Step/Model.hpp>
#include <Media/Step/Commands.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/document/DocumentContext.hpp>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>


namespace Media
{
namespace Step
{
class InspectorWidget final :
    public Process::InspectorWidgetDelegate_T<Model>
{
  public:
    explicit InspectorWidget(
        const Model& obj,
        const score::DocumentContext& doc,
        QWidget* parent):
      InspectorWidgetDelegate_T {obj, parent},
      m_dispatcher{doc.commandStack}
    , m_count{this}
    , m_dur{this}
    , m_min{this}
    , m_max{this}
    {
      m_min.setRange(-100000, 100000);
      m_max.setRange(-100000, 100000);
      m_count.setRange(1, 24);
      m_dur.setRange(1, 1000000);
      m_count.setValue(obj.stepCount());
      m_dur.setValue(obj.stepDuration());
      m_min.setValue(obj.min());
      m_max.setValue(obj.max());

      auto lay = new QFormLayout{this};

      con(process(), &Model::stepCountChanged,
          this, [&] {
        m_count.setValue(obj.stepCount());
      });
      con(process(), &Model::stepDurationChanged,
          this, [&] {
        m_dur.setValue(obj.stepDuration());
      });
      con(process(), &Model::minChanged,
          this, [&] {
        m_min.setValue(obj.min());
      });
      con(process(), &Model::maxChanged,
          this, [&] {
        m_max.setValue(obj.max());
      });

      con(m_count, &QSpinBox::editingFinished,
          this, [&] () {
        m_dispatcher.submitCommand<Commands::SetStepCount>(obj, m_count.value());
        m_dispatcher.commit();
      });
      con(m_dur, &QSpinBox::editingFinished,
          this, [&] () {
        m_dispatcher.submitCommand<Commands::SetStepDuration>(obj, m_dur.value());
        m_dispatcher.commit();
      });
      con(m_min, &QDoubleSpinBox::editingFinished,
          this, [&] () {
        m_dispatcher.submitCommand<Commands::SetMin>(obj, m_min.value());
        m_dispatcher.commit();
      });
      con(m_max, &QDoubleSpinBox::editingFinished,
          this, [&] () {
        m_dispatcher.submitCommand<Commands::SetMax>(obj, m_max.value());
        m_dispatcher.commit();
      });

      lay->addRow(tr("Count"), &m_count);
      lay->addRow(tr("Duration (samples)"), &m_dur);
      lay->addRow(tr("Min"), &m_min);
      lay->addRow(tr("Max"), &m_max);
    }

  private:
    OngoingCommandDispatcher m_dispatcher;

    QSpinBox m_count;
    QSpinBox m_dur;
    QDoubleSpinBox m_min;
    QDoubleSpinBox m_max;

};
class InspectorFactory final :
    public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
    SCORE_CONCRETE("a7c0ecb5-70a1-476c-8187-075076e413d2")
};

}
}

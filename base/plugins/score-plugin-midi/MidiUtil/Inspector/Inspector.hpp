#pragma once
#include <MidiUtil/MidiUtilProcess.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
class QComboBox;
class QSpinBox;
namespace MidiUtil
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<MidiUtil::ProcessModel>
{
public:
  explicit InspectorWidget(
      const ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

private:
};

class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("734861db-5076-46c8-8889-01b565aab409")
};
}

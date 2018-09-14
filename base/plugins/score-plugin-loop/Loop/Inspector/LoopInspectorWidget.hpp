#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <QVBoxLayout>

namespace Loop
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Loop::ProcessModel>
{
public:
  explicit InspectorWidget(
      const Loop::ProcessModel& object, const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;
};
}

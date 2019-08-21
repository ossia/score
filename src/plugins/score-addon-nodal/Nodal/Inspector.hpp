#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Nodal/Process.hpp>

namespace Nodal
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Nodal::Model>
{
public:
  explicit InspectorWidget(
      const Nodal::Model& object, const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;

private:
  CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("2d0829a9-87f1-4bf1-ba9b-d8e5b0f71488")
};
}

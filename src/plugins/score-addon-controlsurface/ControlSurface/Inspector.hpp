#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <ControlSurface/Process.hpp>

namespace ControlSurface
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<ControlSurface::Model>
{
public:
  explicit InspectorWidget(
      const ControlSurface::Model& object, const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;

private:
  CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("b7db07ef-e113-4e18-ac3c-6cc356df3668")
};
}

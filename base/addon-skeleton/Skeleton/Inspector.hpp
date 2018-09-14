#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Skeleton/Process.hpp>

namespace Skeleton
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Skeleton::Model>
{
public:
  explicit InspectorWidget(
      const Skeleton::Model& object, const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget();

private:
  CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("00000000-0000-0000-0000-000000000000")
};
}

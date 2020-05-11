#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Gfx/Filter/Process.hpp>

namespace Gfx::Filter
{
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Gfx::Filter::Model>
{
public:
  explicit InspectorWidget(
      const Gfx::Filter::Model& object,
      const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("f42df1a5-55f6-4d5e-a9fb-15b95c0fafb2")
};
}

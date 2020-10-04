#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Gfx/Images/Process.hpp>

namespace Gfx::Images
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Gfx::Images::Model>
{
public:
  explicit InspectorWidget(
      const Gfx::Images::Model& object,
      const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;

private:
  CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("ee98ee2d-f352-4e7a-8a58-bd01a75de0cd")
};
}

#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Gfx/Mesh/Process.hpp>

namespace Gfx::Mesh
{
class InspectorWidget final : public Process::InspectorWidgetDelegate_T<Gfx::Mesh::Model>
{
public:
  explicit InspectorWidget(
      const Gfx::Mesh::Model& object,
      const score::DocumentContext& context,
      QWidget* parent);
  ~InspectorWidget() override;
};

class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<Model, InspectorWidget>
{
  SCORE_CONCRETE("a6da9f0e-4150-4fec-8a95-89394dd9f2a1")
};
}

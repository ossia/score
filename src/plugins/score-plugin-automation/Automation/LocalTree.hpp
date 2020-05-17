#pragma once
#include <Automation/AutomationModel.hpp>

#include <LocalTree/ProcessComponent.hpp>

namespace LocalTree
{
class AutomationComponent final : public ProcessComponent_T<Automation::ProcessModel>
{
  COMPONENT_METADATA("49d55f75-1ee7-47c9-9a77-450e4da7083c")

public:
  AutomationComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Automation::ProcessModel& proc,
      const score::DocumentContext& ctx,
      QObject* parent_obj)
      : ProcessComponent_T<Automation::ProcessModel>{
          parent,
          proc,
          ctx,
          id,
          "AutomationComponent",
          parent_obj}
  {
    add<Automation::ProcessModel::p_min>(proc);
    add<Automation::ProcessModel::p_max>(proc);
  }
};

using AutomationComponentFactory = ProcessComponentFactory_T<AutomationComponent>;
}

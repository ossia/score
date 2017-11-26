#pragma once
#include <Engine/LocalTree/Scenario/ProcessComponent.hpp>
#include <Automation/AutomationModel.hpp>

namespace Engine
{
namespace LocalTree
{
class AutomationComponent : public ProcessComponent_T<Automation::ProcessModel>
{
  COMPONENT_METADATA("49d55f75-1ee7-47c9-9a77-450e4da7083c")

public:
  AutomationComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Automation::ProcessModel& proc,
      DocumentPlugin& ctx,
      QObject* parent_obj)
      : ProcessComponent_T<Automation::ProcessModel>{
          parent, proc, ctx, id, "AutomationComponent", parent_obj}
  {
    m_properties.push_back(add_property<double>(
        node(), "min", &proc,
        &Automation::ProcessModel::min,
        &Automation::ProcessModel::setMin,
        &Automation::ProcessModel::minChanged, this));
    m_properties.push_back(add_property<double>(
        node(), "max", &proc,
        &Automation::ProcessModel::max,
        &Automation::ProcessModel::setMax,
        &Automation::ProcessModel::maxChanged, this));
  }

private:
  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using AutomationComponentFactory = ProcessComponentFactory_T<AutomationComponent>;
}
}

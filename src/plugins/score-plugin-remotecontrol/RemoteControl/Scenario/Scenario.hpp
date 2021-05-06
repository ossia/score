#pragma once
#include <QMetaObject>

#include <RemoteControl/Scenario/Event.hpp>
#include <RemoteControl/Scenario/Interval.hpp>
#include <RemoteControl/Scenario/State.hpp>
#include <RemoteControl/Scenario/Sync.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace RemoteControl
{
class ScenarioBase : public ProcessComponent_T<Scenario::ProcessModel>
{
  COMPONENT_METADATA("fce752e0-e37a-4b71-bc2a-65366ec87152")

public:
  ScenarioBase(
      Scenario::ProcessModel& scenario,
      DocumentPlugin& doc,
      QObject* parent_obj);

  template <typename Component_T, typename Element>
  Component_T* make(Element& elt)
  {
    return new Component_T{elt, system(), this};
  }

  template <typename... Args>
  bool removing(Args&&...)
  {
    return true;
  }
  template <typename... Args>
  void removed(Args&&...)
  {
  }
};

using ScenarioComponent = HierarchicalScenarioComponent<
    ScenarioBase,
    Scenario::ProcessModel,
    Interval,
    Event,
    Sync,
    State>;

using ScenarioComponentFactory = ProcessComponentFactory_T<ScenarioComponent>;
}

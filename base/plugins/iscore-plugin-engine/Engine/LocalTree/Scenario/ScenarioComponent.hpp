#pragma once
#include <Engine/LocalTree/Scenario/IntervalComponent.hpp>
#include <Engine/LocalTree/Scenario/EventComponent.hpp>
#include <Engine/LocalTree/Scenario/StateComponent.hpp>
#include <Engine/LocalTree/Scenario/TimeSyncComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace Engine
{
namespace LocalTree
{
class ScenarioComponentBase : public ProcessComponent_T<Scenario::ProcessModel>
{
  COMPONENT_METADATA("57c37324-f5a5-494e-8b45-206750d9fa77")

public:
  ScenarioComponentBase(
      const Id<iscore::Component>& id,
      ossia::net::node_base& parent,
      Scenario::ProcessModel& scenario,
      DocumentPlugin& doc,
      QObject* parent_obj);

  template <typename Component_T, typename Element>
  Component_T* make(const Id<iscore::Component>& id, Element& elt);

  template<typename... Args>
  bool removing(Args&&...) { return true; }
  template<typename... Args>
  void removed(Args&&...) { }

private:
  ossia::net::node_base& m_intervalsNode;
  ossia::net::node_base& m_eventsNode;
  ossia::net::node_base& m_timeSyncsNode;
  ossia::net::node_base& m_statesNode;

  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using ScenarioComponent
    = HierarchicalScenarioComponent<ScenarioComponentBase, Scenario::ProcessModel, Interval, Event, TimeSync, State>;

using ScenarioComponentFactory
    = Engine::LocalTree::ProcessComponentFactory_T<ScenarioComponent>;
}
}

#pragma once
#include <Engine/LocalTree/Scenario/EventComponent.hpp>
#include <Engine/LocalTree/Scenario/IntervalComponent.hpp>
#include <Engine/LocalTree/Scenario/StateComponent.hpp>
#include <Engine/LocalTree/Scenario/TimeSyncComponent.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace Engine
{
namespace LocalTree
{
class LoopComponentBase : public ProcessComponent_T<Loop::ProcessModel>
{
  COMPONENT_METADATA("091651dd-bd98-4b85-9ae5-44cb0453cced")

public:
  LoopComponentBase(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Loop::ProcessModel& loop,
      DocumentPlugin& sys,
      QObject* parent_obj);

  template <typename Component_T, typename Element>
  Component_T* make(const Id<score::Component>& id, Element& elt);

  template <typename... Args>
  void removing(Args&&...)
  {
  }

private:
  ossia::net::node_base& m_intervalsNode;
  ossia::net::node_base& m_eventsNode;
  ossia::net::node_base& m_timeSyncsNode;
  ossia::net::node_base& m_statesNode;

  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using LoopComponent = HierarchicalBaseScenario<
    LoopComponentBase,
    Loop::ProcessModel,
    Interval,
    Event,
    TimeSync,
    State>;

using LoopComponentFactory = ProcessComponentFactory_T<LoopComponent>;
}
}

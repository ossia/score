#pragma once
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Engine/LocalTree/Scenario/ProcessComponent.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <score/model/ComponentHierarchy.hpp>
namespace Engine
{
namespace LocalTree
{
class IntervalBase
    : public Component<Scenario::GenericIntervalComponent<DocumentPlugin>>
{
  COMMON_COMPONENT_METADATA("11d928b5-eaeb-471c-b3b7-dc453180b10f")
public:
  using parent_t
      = Component<Scenario::GenericIntervalComponent<DocumentPlugin>>;
  using model_t = Process::ProcessModel;
  using component_t = Engine::LocalTree::ProcessComponent;
  using component_factory_t = Engine::LocalTree::ProcessComponentFactory;
  using component_factory_list_t
      = Engine::LocalTree::ProcessComponentFactoryList;

  IntervalBase(
      ossia::net::node_base& parent,
      const Id<score::Component>& id,
      Scenario::IntervalModel& interval,
      DocumentPlugin& sys,
      QObject* parent_comp);

  ProcessComponent* make(
      const Id<score::Component>& id,
      ProcessComponentFactory& factory,
      Process::ProcessModel& process);

  bool
  removing(const Process::ProcessModel& cst, const ProcessComponent& comp);
  template<typename... Args>
  void added(Args&&...) { }
  template<typename... Args>
  void removed(Args&&...) { }

private:
  ossia::net::node_base& m_processesNode;
};

class Interval final
    : public score::PolymorphicComponentHierarchy<IntervalBase>
{
public:
  template <typename... Args>
  Interval(Args&&... args)
      : score::PolymorphicComponentHierarchy<IntervalBase>{
            std::forward<Args>(args)...}
  {
  }
};
}
}

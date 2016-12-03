#pragma once
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Engine/LocalTree/Scenario/ProcessComponent.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>
#include <iscore/component/ComponentHierarchy.hpp>
namespace Engine
{
namespace LocalTree
{
class ConstraintBase
    : public Component<Scenario::GenericConstraintComponent<DocumentPlugin>>
{
  COMMON_COMPONENT_METADATA("11d928b5-eaeb-471c-b3b7-dc453180b10f")
public:
  using parent_t
      = Component<Scenario::GenericConstraintComponent<DocumentPlugin>>;
  using model_t = Process::ProcessModel;
  using component_t = Engine::LocalTree::ProcessComponent;
  using component_factory_t = Engine::LocalTree::ProcessComponentFactory;
  using component_factory_list_t
      = Engine::LocalTree::ProcessComponentFactoryList;

  ConstraintBase(
      ossia::net::node_base& parent,
      const Id<iscore::Component>& id,
      Scenario::ConstraintModel& constraint,
      DocumentPlugin& sys,
      QObject* parent_comp);

  ProcessComponent* make(
      const Id<iscore::Component>& id,
      ProcessComponentFactory& factory,
      Process::ProcessModel& process);

  void
  removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

private:
  ossia::net::node_base& m_processesNode;
};

class Constraint final
    : public iscore::PolymorphicComponentHierarchy<ConstraintBase>
{
public:
  template <typename... Args>
  Constraint(Args&&... args)
      : iscore::PolymorphicComponentHierarchy<ConstraintBase>{
            std::forward<Args>(args)...}
  {
  }
};
}
}

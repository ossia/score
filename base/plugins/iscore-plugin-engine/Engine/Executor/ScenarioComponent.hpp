#pragma once
#include <ossia/editor/scenario/time_event.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <map>
#include <memory>
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeNodeComponent.hpp>

#include "ProcessComponent.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

namespace Device
{
class DeviceList;
}
namespace Process
{
class ProcessModel;
}
class QObject;
namespace ossia
{
class time_process;
struct time_value;
} // namespace OSSIA
namespace Engine
{
namespace Execution
{
class EventComponent;
class StateComponent;
class TimeNodeComponent;
}
} // namespace RecreateOnPlay
namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class CSPCoherencyCheckerInterface;
} // namespace Scenario

namespace ossia
{
class scenario;
}

namespace Engine
{
namespace Execution
{
class ConstraintComponent;

// TODO see if this can be used for the base scenario model too.
class ScenarioComponentBase
    : public ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>
{
  COMPONENT_METADATA("4e4b1c1a-1a2a-4ae6-a1a1-38d0900e74e8")

  friend class EventInitCommand;
public:
  ScenarioComponentBase(
      ConstraintComponent& cst,
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  const auto& states() const
  {
    return m_ossia_states;
  }
  const iscore::hash_map<Id<Scenario::ConstraintModel>, std::shared_ptr<ConstraintComponent>>& constraints() const
  {
    return m_ossia_constraints;
  }
  const auto& events() const
  {
    return m_ossia_timeevents;
  }
  const auto& timeNodes() const
  {
    return m_ossia_timenodes;
  }

  void stop() override;

  template <typename Component_T, typename Element>
  Component_T* make(const Id<iscore::Component>& id, Element& elt);

  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if(f)
      f();
  }


  std::function<void()> removing(const Scenario::ConstraintModel& e, ConstraintComponent& c);

  std::function<void()> removing(const Scenario::TimeNodeModel& e, TimeNodeComponent& c);

  std::function<void()> removing(const Scenario::EventModel& e, EventComponent& c);

  std::function<void()> removing(const Scenario::StateModel& e, StateComponent& c);
protected:
  void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
  void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);
  void disableConstraintExecution(const Id<Scenario::ConstraintModel>& id);

  void eventCallback(EventComponent& ev, ossia::time_event::status newStatus);

  void timeNodeCallback(
      Engine::Execution::TimeNodeComponent* tn, ossia::time_value date);

  iscore::hash_map<Id<Scenario::ConstraintModel>, std::shared_ptr<ConstraintComponent>>
      m_ossia_constraints;
  iscore::hash_map<Id<Scenario::StateModel>, std::shared_ptr<StateComponent>>
      m_ossia_states;
  iscore::hash_map<Id<Scenario::TimeNodeModel>, std::shared_ptr<TimeNodeComponent>>
      m_ossia_timenodes;
  iscore::hash_map<Id<Scenario::EventModel>, std::shared_ptr<EventComponent>>
      m_ossia_timeevents;

  iscore::hash_map<Id<Scenario::ConstraintModel>, Scenario::ConstraintModel*>
      m_executingConstraints;

  const Context& m_ctx;

  Scenario::CSPCoherencyCheckerInterface* m_checker{};
  QVector<Id<Scenario::TimeNodeModel>> m_pastTn{};
  Scenario::ElementsProperties m_properties{};
};

using ScenarioComponentHierarchy
    = HierarchicalScenarioComponent<
        ScenarioComponentBase,
        Scenario::ProcessModel, ConstraintComponent, EventComponent, TimeNodeComponent, StateComponent, false>;

struct ScenarioComponent final : public ScenarioComponentHierarchy
{
  ScenarioComponent(
      ConstraintComponent& cst,
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  void init();

  void cleanup() override;
};

struct ScenarioComponentFactory final :
    public ::Engine::Execution::ProcessComponentFactory_T<ScenarioComponent>
{
  void init(ProcessComponent* comp) const override
  {
    if(comp)
    {
      auto s = static_cast<ScenarioComponent*>(comp);
      s->init();
    }
  }
};
}}

#pragma once
#include <ossia/editor/scenario/time_event.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>
#include <map>
#include <memory>
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeSyncComponent.hpp>

#include "ProcessComponent.hpp"
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

Q_DECLARE_METATYPE(std::shared_ptr<Engine::Execution::EventComponent>)
Q_DECLARE_METATYPE(ossia::time_event::status)
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
class TimeSyncComponent;
}
} // namespace RecreateOnPlay
namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
class TimeSyncModel;
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
class IntervalComponent;
// TODO see if this can be used for the base scenario model too.
class ScenarioComponentBase
    : public ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>
{
  Q_OBJECT
  COMPONENT_METADATA("4e4b1c1a-1a2a-4ae6-a1a1-38d0900e74e8")

public:
  ScenarioComponentBase(
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~ScenarioComponentBase() override;

  const auto& states() const
  {
    return m_ossia_states;
  }
  const score::hash_map<Id<Scenario::IntervalModel>, std::shared_ptr<IntervalComponent>>& intervals() const
  {
    return m_ossia_intervals;
  }
  const auto& events() const
  {
    return m_ossia_timeevents;
  }
  const auto& timeSyncs() const
  {
    return m_ossia_timesyncs;
  }

  void stop() override;

  template <typename Component_T, typename Element>
  Component_T* make(const Id<score::Component>& id, Element& elt);

  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if(f)
      f();
  }


  std::function<void()> removing(const Scenario::IntervalModel& e, IntervalComponent& c);

  std::function<void()> removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c);

  std::function<void()> removing(const Scenario::EventModel& e, EventComponent& c);

  std::function<void()> removing(const Scenario::StateModel& e, StateComponent& c);

signals:
  void sig_eventCallback(std::shared_ptr<EventComponent>, ossia::time_event::status st);
protected:
  void startIntervalExecution(const Id<Scenario::IntervalModel>&);
  void stopIntervalExecution(const Id<Scenario::IntervalModel>&);
  void disableIntervalExecution(const Id<Scenario::IntervalModel>& id);

  void eventCallback(std::shared_ptr<EventComponent> ev, ossia::time_event::status newStatus);

  void timeSyncCallback(
      Engine::Execution::TimeSyncComponent* tn, ossia::time_value date);

  score::hash_map<Id<Scenario::IntervalModel>, std::shared_ptr<IntervalComponent>>
      m_ossia_intervals;
  score::hash_map<Id<Scenario::StateModel>, std::shared_ptr<StateComponent>>
      m_ossia_states;
  score::hash_map<Id<Scenario::TimeSyncModel>, std::shared_ptr<TimeSyncComponent>>
      m_ossia_timesyncs;
  score::hash_map<Id<Scenario::EventModel>, std::shared_ptr<EventComponent>>
      m_ossia_timeevents;

  score::hash_map<Id<Scenario::IntervalModel>, Scenario::IntervalModel*>
      m_executingIntervals;

  const Context& m_ctx;

  Scenario::CSPCoherencyCheckerInterface* m_checker{};
  QVector<Id<Scenario::TimeSyncModel>> m_pastTn{};
  Scenario::ElementsProperties m_properties{};
};

using ScenarioComponentHierarchy
    = HierarchicalScenarioComponent<
        ScenarioComponentBase,
        Scenario::ProcessModel, IntervalComponent, EventComponent, TimeSyncComponent, StateComponent, false>;

struct ScenarioComponent final : public ScenarioComponentHierarchy
{
  ScenarioComponent(
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
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

#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Graph.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Tools/dataStructures.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/editor/scenario/time_event.hpp>

#include <memory>
#include <verdigris>

Q_DECLARE_METATYPE(std::shared_ptr<Execution::EventComponent>)
W_REGISTER_ARGTYPE(std::shared_ptr<Execution::EventComponent>)
Q_DECLARE_METATYPE(ossia::time_event::status)
W_REGISTER_ARGTYPE(ossia::time_event::status)
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

namespace Execution
{
class EventComponent;
class StateComponent;
class TimeSyncComponent;
}

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

namespace Execution
{
class IntervalComponent;
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioComponentBase
    : public ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>
{
  W_OBJECT(ScenarioComponentBase)
  COMPONENT_METADATA("4e4b1c1a-1a2a-4ae6-a1a1-38d0900e74e8")

public:
  ScenarioComponentBase(
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~ScenarioComponentBase() override;

  const auto& states() const { return m_ossia_states; }
  const score::hash_map<Id<Scenario::IntervalModel>, std::shared_ptr<IntervalComponent>>&
  intervals() const
  {
    return m_ossia_intervals;
  }
  const auto& events() const { return m_ossia_timeevents; }
  const auto& timeSyncs() const { return m_ossia_timesyncs; }

  void stop() override;

  template <typename Component_T, typename Element>
  Component_T* make(const Id<score::Component>& id, Element& elt);

  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if (f)
      f();
  }

  std::function<void()> removing(const Scenario::IntervalModel& e, IntervalComponent& c);

  std::function<void()> removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c);

  std::function<void()> removing(const Scenario::EventModel& e, EventComponent& c);

  std::function<void()> removing(const Scenario::StateModel& e, StateComponent& c);

public:
  void sig_eventCallback(std::shared_ptr<EventComponent> arg_1, ossia::time_event::status st)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, sig_eventCallback, arg_1, st)

protected:
  void startIntervalExecution(const Id<Scenario::IntervalModel>&);
  void stopIntervalExecution(const Id<Scenario::IntervalModel>&);
  void disableIntervalExecution(const Id<Scenario::IntervalModel>& id);

  void eventCallback(std::shared_ptr<EventComponent> ev, ossia::time_event::status newStatus);

  void timeSyncCallback(Execution::TimeSyncComponent* tn, ossia::time_value date);

  score::hash_map<Id<Scenario::IntervalModel>, std::shared_ptr<IntervalComponent>>
      m_ossia_intervals;
  score::hash_map<Id<Scenario::StateModel>, std::shared_ptr<StateComponent>> m_ossia_states;
  score::hash_map<Id<Scenario::TimeSyncModel>, std::shared_ptr<TimeSyncComponent>>
      m_ossia_timesyncs;
  score::hash_map<Id<Scenario::EventModel>, std::shared_ptr<EventComponent>> m_ossia_timeevents;

  score::hash_map<Id<Scenario::IntervalModel>, Scenario::IntervalModel*> m_executingIntervals;

  const Context& m_ctx;

  Scenario::CSPCoherencyCheckerInterface* m_checker{};
  QVector<Id<Scenario::TimeSyncModel>> m_pastTn{};
  Scenario::ElementsProperties m_properties{};

  Scenario::TimenodeGraph m_graph;
  /*
  struct GhostInterval {
    std::shared_ptr<ossia::time_interval> interval;
    std::shared_ptr<ossia::time_event> end_event;
  };

  std::vector<GhostInterval> m_ghosts;
  std::shared_ptr<ossia::time_event> m_ghost_start;
  */
};

using ScenarioComponentHierarchy = HierarchicalScenarioComponent<
    ScenarioComponentBase,
    Scenario::ProcessModel,
    IntervalComponent,
    EventComponent,
    TimeSyncComponent,
    StateComponent,
    false>;

struct SCORE_PLUGIN_SCENARIO_EXPORT ScenarioComponent final : public ScenarioComponentHierarchy
{
  ScenarioComponent(
      Scenario::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void lazy_init() override;

  void cleanup() override;
};

using ScenarioComponentFactory = Execution::ProcessComponentFactory_T<ScenarioComponent>;
}

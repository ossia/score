#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/tools/std/HashMap.hpp>

#include <ossia/editor/scenario/time_event.hpp>

#include <memory>
#include <verdigris>

namespace Execution
{
class SequenceComponentBase;
}
Q_DECLARE_METATYPE(std::weak_ptr<Execution::SequenceComponentBase>)
W_REGISTER_ARGTYPE(std::weak_ptr<Execution::SequenceComponentBase>)

namespace Execution
{

class SCORE_PLUGIN_SCENARIO_EXPORT SequenceComponentBase
    : public ProcessComponent_T<Sequence::SequenceModel, ossia::scenario>
{
  W_OBJECT(SequenceComponentBase)
  COMPONENT_METADATA("b2e3c4d5-e6f7-4a8b-9c0d-1e2f3a4b5c6d")

public:
  SequenceComponentBase(
      Sequence::SequenceModel& proc, const Context& ctx, QObject* parent);
  ~SequenceComponentBase() override;

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
  Component_T* make(Element& elt);

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

public:
  void sig_eventCallback(
      std::weak_ptr<SequenceComponentBase> self, std::shared_ptr<EventComponent> arg_1,
      ossia::time_event::status st)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, sig_eventCallback, self, arg_1, st)

protected:
  void startIntervalExecution(const Id<Scenario::IntervalModel>&);
  void stopIntervalExecution(const Id<Scenario::IntervalModel>&);
  void disableIntervalExecution(const Id<Scenario::IntervalModel>& id);

  void eventCallback(
      std::weak_ptr<SequenceComponentBase> self, std::shared_ptr<EventComponent> ev,
      ossia::time_event::status newStatus);

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
};

using SequenceComponentHierarchy = HierarchicalScenarioComponent<
    SequenceComponentBase, Sequence::SequenceModel, IntervalComponent, EventComponent,
    TimeSyncComponent, StateComponent, false>;

struct SCORE_PLUGIN_SCENARIO_EXPORT SequenceComponent final
    : public SequenceComponentHierarchy
{
  SequenceComponent(
      Sequence::SequenceModel& proc, const Context& ctx, QObject* parent);

  void lazy_init() override;

  void cleanup() override;
};

using SequenceComponentFactory = Execution::ProcessComponentFactory_T<SequenceComponent>;

}

#pragma once
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

#include <LocalTree/ScriptableProcessComponent.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

#include <nano_observer.hpp>

#include <atomic>
#include <thread>

namespace ossia
{
class time_sync;
}

namespace LocalTree
{
//! score:/triggers/<name>: an impulse that fires the sync.
class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableTimeSync final
    : public score::GenericComponent<const score::DocumentContext>
{
  COMMON_COMPONENT_METADATA("eeb9acf5-466a-473d-baf9-861427364dcf")
public:
  static constexpr bool is_unique = true;
  ScriptableTimeSync(
      ScriptableRoots roots, Scenario::TimeSyncModel& sync,
      const score::DocumentContext& ctx, QObject* parent);
  ~ScriptableTimeSync();

  ossia::net::node_base* node() const noexcept { return m_node; }

  //! Lets a trigger from any thread reach the running sync; null when not playing
  struct Execution
  {
    //! detach() waits for the triggers in flight; only the GUI thread calls it
    void attach(ossia::time_sync* s) noexcept { m_sync.store(s); }
    void detach() noexcept
    {
      m_sync.store(nullptr);
      while(m_users.load() != 0)
        std::this_thread::yield();
    }
    ossia::time_sync* attached() const noexcept { return m_sync.load(); }
    //! False when no execution is attached
    bool trigger() noexcept
    {
      m_users.fetch_add(1);
      auto sync = m_sync.load();
      if(sync)
        sync->start_trigger_request();
      m_users.fetch_sub(1);
      return sync;
    }

  private:
    std::atomic<ossia::time_sync*> m_sync{};
    std::atomic_int m_users{};
  };
  const std::shared_ptr<Execution>& execution() const noexcept { return m_execution; }

private:
  void refresh();

  ScriptableRoots m_roots;
  Scenario::TimeSyncModel& m_sync;
  ossia::net::node_base* m_node{};
  GivenName m_given;
  std::unique_ptr<ParameterBinding> m_trigger;
  std::shared_ptr<Execution> m_execution = std::make_shared<Execution>();
};

//! score:/conditions/<name>: a boolean for the event's expression to read.
class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableEvent final
    : public score::GenericComponent<const score::DocumentContext>
{
  COMMON_COMPONENT_METADATA("90d9d6d6-9b98-4e2e-9c4b-8d834706472e")
public:
  static constexpr bool is_unique = true;
  ScriptableEvent(
      ScriptableRoots roots, Scenario::EventModel& event,
      const score::DocumentContext& ctx, QObject* parent);
  ~ScriptableEvent();

  ossia::net::node_base* node() const noexcept { return m_node; }

private:
  void refresh();

  ScriptableRoots m_roots;
  Scenario::EventModel& m_event;
  ossia::net::node_base* m_node{};
  GivenName m_given;
};

//! Registers the state in the ReferenceIndex, since it holds messages.
class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableState final
    : public score::GenericComponent<const score::DocumentContext>
{
  COMMON_COMPONENT_METADATA("b03c26f1-9f6d-4dcd-bdbd-6c63d70eee62")
public:
  static constexpr bool is_unique = true;
  ScriptableState(
      ScriptableRoots roots, Scenario::StateModel& state,
      const score::DocumentContext& ctx, QObject* parent);
};

//! Tracks the processes of an interval, and the hierarchy below a scenario.
class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableInterval final
    : public Scenario::GenericIntervalComponent<const score::DocumentContext>
    , public Nano::Observer
{
  COMMON_COMPONENT_METADATA("b49b38e4-5b79-4f24-8c58-873d8987b98e")
public:
  static constexpr bool is_unique = true;

  ScriptableInterval(
      ScriptableRoots roots, Scenario::IntervalModel& interval,
      const score::DocumentContext& ctx, QObject* parent);
  ~ScriptableInterval();

private:
  void add(Process::ProcessModel& proc);
  void remove(const Process::ProcessModel& proc);

  ScriptableRoots m_roots;
  std::vector<std::pair<Process::ProcessModel*, ScriptableProcessBase*>> m_children;
};

class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableScenarioBase
    : public Process::GenericProcessComponent_T<ScriptableProcessBase, Scenario::ProcessModel>
{
  COMPONENT_METADATA("4084de06-cbb0-4e3c-915f-6337946f37e2")
public:
  ScriptableScenarioBase(
      ScriptableRoots roots, Scenario::ProcessModel& scenario,
      const score::DocumentContext& ctx, QObject* parent);

  template <typename Component_T, typename Element>
  Component_T* make(Element& elt);

  template <typename... Args>
  bool removing(Args&&...)
  {
    return true;
  }
  template <typename... Args>
  void removed(Args&&...)
  {
  }

private:
  ScriptableRoots m_roots;
};

using ScriptableScenario = HierarchicalScenarioComponent<
    ScriptableScenarioBase, Scenario::ProcessModel, ScriptableInterval, ScriptableEvent,
    ScriptableTimeSync, ScriptableState>;

class SCORE_PLUGIN_SCENARIO_EXPORT ScriptableScenarioFactory final
    : public ScriptableProcessFactory
{
  SCORE_CONCRETE("74f159f0-0ab8-4a3f-bf2a-16a286d95c54")
public:
  bool matches(const Process::ProcessModel& proc) const noexcept override;
  ScriptableProcessBase* make(
      ScriptableRoots roots, Process::ProcessModel& proc,
      const score::DocumentContext& ctx, QObject* parent) const override;
};

SCORE_PLUGIN_SCENARIO_EXPORT ::State::Address scriptableAddress(const Scenario::TimeSyncModel&);
SCORE_PLUGIN_SCENARIO_EXPORT ::State::Address scriptableAddress(const Scenario::EventModel&);
}

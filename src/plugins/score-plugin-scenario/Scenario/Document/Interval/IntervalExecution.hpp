#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>

#include <score/model/ComponentHierarchy.hpp>
#include <score/model/Identifier.hpp>

#include <ossia-qt/time.hpp>

#include <QObject>

#include <verdigris>

#include <memory>

namespace Process
{
class ProcessModel;
}
namespace score
{
struct DocumentContext;
}
namespace ossia
{
class loop;
class time_interval;
}
namespace Scenario
{
class IntervalModel;
}

namespace Execution
{
class IntervalComponentBase;
class IntervalComponent;
}

namespace score
{
#if defined(SCORE_SERIALIZABLE_COMPONENTS)
template <>
struct is_component_serializable<Execution::IntervalComponentBase>
{
  using type = score::not_serializable_tag;
};

template <>
struct is_component_serializable<Execution::IntervalComponent>
{
  using type = score::not_serializable_tag;
};
#endif
}

namespace Execution
{
struct Context;

struct interval_duration_data
{
  ossia::time_value defaultDuration;
  ossia::time_value minDuration;
  ossia::time_value maxDuration;
  double speed;
};

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalComponentBase
    : public Scenario::GenericIntervalComponent<const Context>
{
  COMMON_COMPONENT_METADATA("4d644678-1924-49bf-8c82-89841581d23f")
public:
  using parent_t = Execution::Component;
  using model_t = Process::ProcessModel;
  using component_t = ProcessComponent;
  using component_factory_list_t = ProcessComponentFactoryList;

  static const constexpr bool is_unique = true;
  IntervalComponentBase(
      Scenario::IntervalModel& score_cst,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  IntervalComponentBase(const IntervalComponentBase&) = delete;
  IntervalComponentBase(IntervalComponentBase&&) = delete;
  IntervalComponentBase& operator=(const IntervalComponentBase&) = delete;
  IntervalComponentBase& operator=(IntervalComponentBase&&) = delete;

  //! To be called from the GUI thread
  interval_duration_data makeDurations() const;

  const std::shared_ptr<ossia::time_interval>& OSSIAInterval() const;
  Scenario::IntervalModel& scoreInterval() const;

  const auto& processes() const { return m_processes; }

  void pause();
  void resume();
  void stop();

  void executionStarted();
  void executionStopped();

  ProcessComponent* make(
      const Id<score::Component>& id,
      ProcessComponentFactory& factory,
      Process::ProcessModel& process);
  ProcessComponent*
  make(const Id<score::Component>& id, Process::ProcessModel& process)
  {
    return nullptr;
  }
  std::function<void()>
  removing(const Process::ProcessModel& e, ProcessComponent& c);

  template <typename... Args>
  void added(Args&&...)
  {
  }
  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if (f)
      f();
  }

  const Context& context() const { return system(); }

protected:
  void on_processAdded(Process::ProcessModel& score_proc);
  void recomputePropagate(const Process::ProcessModel& process, const Process::Port& port);

  std::shared_ptr<ossia::time_interval> m_ossia_interval;
  score::hash_map<Id<Process::ProcessModel>, std::shared_ptr<ProcessComponent>>
      m_processes;
};

class SCORE_PLUGIN_SCENARIO_EXPORT IntervalComponent final
    : public score::PolymorphicComponentHierarchy<IntervalComponentBase, false>
{
  W_OBJECT(IntervalComponent)

public:
  template <typename... Args>
  IntervalComponent(Args&&... args)
      : PolymorphicComponentHierarchyManager{score::lazy_init_t{},
                                             std::forward<Args>(args)...}
  {
  }

  IntervalComponent(const IntervalComponent&) = delete;
  IntervalComponent(IntervalComponent&&) = delete;
  IntervalComponent& operator=(const IntervalComponent&) = delete;
  IntervalComponent& operator=(IntervalComponent&&) = delete;
  ~IntervalComponent();

  // only here to help autocompletion
  const std::shared_ptr<ossia::time_interval>& OSSIAInterval() const
  {
    return IntervalComponentBase::OSSIAInterval();
  }

  void init();
  void cleanup(const std::shared_ptr<IntervalComponent>&);

  //! To be called from the API edition thread
  void onSetup(
      std::shared_ptr<IntervalComponent>,
      std::shared_ptr<ossia::time_interval> ossia_cst,
      interval_duration_data dur);

public:
  void slot_callback(bool running, ossia::time_value date);
  W_SLOT(slot_callback);
  void graph_slot_callback(bool running, ossia::time_value date);
  W_SLOT(graph_slot_callback);
};
}

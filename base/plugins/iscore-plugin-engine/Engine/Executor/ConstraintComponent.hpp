#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <iscore/model/Identifier.hpp>
#include <memory>
#include <iscore/model/ComponentHierarchy.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <iscore_plugin_engine_export.h>

namespace Process
{
class ProcessModel;
}
namespace iscore
{
struct DocumentContext;
}
namespace ossia
{
class loop;
class time_constraint;
}
namespace Scenario
{
class ConstraintModel;
}

namespace Engine
{
namespace Execution
{
struct Context;
class DocumentPlugin;
class ISCORE_PLUGIN_ENGINE_EXPORT ConstraintComponentBase :
    public Scenario::GenericConstraintComponent<const Context>
{
  COMMON_COMPONENT_METADATA("4d644678-1924-49bf-8c82-89841581d23f")
public:
    using parent_t = Engine::Execution::Component;
    using model_t = Process::ProcessModel;
    using component_t = ProcessComponent;
    using component_factory_list_t = ProcessComponentFactoryList;

  static const constexpr bool is_unique = true;
  ConstraintComponentBase(
      Scenario::ConstraintModel& iscore_cst,
      const Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);
  ConstraintComponentBase(const ConstraintComponentBase&) = delete;
  ConstraintComponentBase(ConstraintComponentBase&&) = delete;
  ConstraintComponentBase& operator=(const ConstraintComponentBase&) = delete;
  ConstraintComponentBase& operator=(ConstraintComponentBase&&) = delete;
  ~ConstraintComponentBase();

  struct constraint_duration_data
  {
    ossia::time_value defaultDuration;
    ossia::time_value minDuration;
    ossia::time_value maxDuration;
    double speed;
  };

  //! To be called from the GUI thread
  void play(TimeValue t = TimeValue::zero());

  //! To be called from the GUI thread
  constraint_duration_data makeDurations() const;

  std::shared_ptr<ossia::time_constraint> OSSIAConstraint() const;
  Scenario::ConstraintModel& iscoreConstraint() const;

  const auto& processes() const { return m_processes; }

  void pause();
  void resume();
  void stop();

  void executionStarted();
  void executionStopped();


  ProcessComponent* make(
          const Id<iscore::Component> & id,
          ProcessComponentFactory& factory,
          Process::ProcessModel &process);
  std::function<void()> removing(
      const Process::ProcessModel& e,
      ProcessComponent& c);

  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if(f)
      f();
  }


  auto& context() { return system(); }
protected:
  void on_processAdded(Process::ProcessModel& iscore_proc);

  std::shared_ptr<ossia::time_constraint> m_ossia_constraint;
  std::vector<std::shared_ptr<ProcessComponent>> m_processes;
};


class ISCORE_PLUGIN_ENGINE_EXPORT ConstraintComponent final :
        public iscore::PolymorphicComponentHierarchy<ConstraintComponentBase, false>
{
    public:
  template<typename... Args>
  ConstraintComponent(Args&&... args):
    PolymorphicComponentHierarchyManager{
      iscore::lazy_init_t{}, std::forward<Args>(args)...}
  {

  }
  void init();
  void cleanup();

  //! To be called from the API edition thread
  void onSetup(std::shared_ptr<ossia::time_constraint> ossia_cst,
               constraint_duration_data dur,
               bool parent_is_base_scenario);

};
}
}

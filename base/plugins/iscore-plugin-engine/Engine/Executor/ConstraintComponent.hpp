#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <iscore/model/Identifier.hpp>
#include <memory>

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
class ISCORE_PLUGIN_ENGINE_EXPORT ConstraintComponent final :
    public Component
{
  COMMON_COMPONENT_METADATA("4d644678-1924-49bf-8c82-89841581d23f")
public:
  static const constexpr bool is_unique = true;
  ConstraintComponent(
      Scenario::ConstraintModel& iscore_cst,
      const Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);
  ~ConstraintComponent();

  void init();
  void cleanup();

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

  //! To be called from the API edition thread
  void onSetup(std::shared_ptr<ossia::time_constraint> ossia_cst,
               constraint_duration_data dur,
               bool parent_is_base_scenario);

  std::shared_ptr<ossia::time_constraint> OSSIAConstraint() const;
  Scenario::ConstraintModel& iscoreConstraint() const;

  const auto& processes() const { return m_processes; }

  void pause();
  void resume();
  void stop();

  void executionStarted();
  void executionStopped();

private:
  void on_processAdded(Process::ProcessModel& iscore_proc);
  void constraintCallback(
      ossia::time_value position,
      ossia::time_value date,
      const ossia::state& state);

  Scenario::ConstraintModel& m_iscore_constraint;
  std::shared_ptr<ossia::time_constraint> m_ossia_constraint;

  std::vector<std::shared_ptr<ProcessComponent>> m_processes;
};
}
}

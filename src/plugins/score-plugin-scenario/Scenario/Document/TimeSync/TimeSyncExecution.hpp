#pragma once
#include <Process/ExecutionComponent.hpp>

#include <ossia/editor/expression/expression.hpp>

#include <score_plugin_scenario_export.h>
namespace ossia
{
class time_sync;
struct time_value;
}
namespace Scenario
{
class TimeSyncModel;
}

namespace Execution
{
class SCORE_PLUGIN_SCENARIO_EXPORT TimeSyncComponent final : public Execution::Component
{
  COMMON_COMPONENT_METADATA("eca86942-002e-4af5-ad3d-cb1615e0552c")
public:
  static const constexpr bool is_unique = true;
  TimeSyncComponent(
      const Scenario::TimeSyncModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void cleanup();

  //! To be called from the GUI thread
  ossia::expression_ptr makeTrigger() const;

  //! To be called from the API edition queue
  void onSetup(std::shared_ptr<ossia::time_sync> ptr, ossia::expression_ptr exp);

  std::shared_ptr<ossia::time_sync> OSSIATimeSync() const;
  const Scenario::TimeSyncModel& scoreTimeSync() const;

private:
  void updateTrigger();
  void updateTriggerTime();
  void on_GUITrigger();
  std::shared_ptr<ossia::time_sync> m_ossia_node;
  QPointer<const Scenario::TimeSyncModel> m_score_node;
};
}

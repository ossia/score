#pragma once
#include <Engine/Executor/Component.hpp>
#include <ossia/editor/expression/expression.hpp>
namespace ossia
{
class time_node;
class time_value;
}
namespace Scenario
{
class TimeNodeModel;
}

namespace Engine
{
namespace Execution
{
class ISCORE_PLUGIN_ENGINE_EXPORT TimeNodeComponent final :
    public Execution::Component
{
  COMMON_COMPONENT_METADATA("eca86942-002e-4af5-ad3d-cb1615e0552c")
public:
  static const constexpr bool is_unique = true;
  TimeNodeComponent(
      const Scenario::TimeNodeModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  void cleanup();

  //! To be called from the GUI thread
  ossia::expression_ptr makeTrigger() const;

  //! To be called from the API edition queue
  void onSetup(std::shared_ptr<ossia::time_node> ptr,
               ossia::expression_ptr exp);

  std::shared_ptr<ossia::time_node> OSSIATimeNode() const;
  const Scenario::TimeNodeModel& iscoreTimeNode() const;

private:
  void updateTrigger();
  void on_GUITrigger();
  std::shared_ptr<ossia::time_node> m_ossia_node;
  const Scenario::TimeNodeModel& m_iscore_node;
};
}
}

#pragma once
#include <Engine/Executor/Component.hpp>
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
  TimeNodeComponent(
      const Scenario::TimeNodeModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  void onSetup(std::shared_ptr<ossia::time_node> ptr);

  std::shared_ptr<ossia::time_node> OSSIATimeNode() const;
  const Scenario::TimeNodeModel& iscoreTimeNode() const;

private:
  std::shared_ptr<ossia::time_node> m_ossia_node;
  const Scenario::TimeNodeModel& m_iscore_node;
};
}
}

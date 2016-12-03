#pragma once
#include <QObject>
#include <iscore_plugin_engine_export.h>
#include <memory>

namespace Device
{
class DeviceList;
}

namespace ossia
{
class time_event;
}
namespace Scenario
{
class StateModel;
}
namespace Engine
{
namespace Execution
{
struct Context;
class ISCORE_PLUGIN_ENGINE_EXPORT StateElement final : public QObject
{
public:
  StateElement(
      const Scenario::StateModel& element,
      ossia::time_event& root,
      const Engine::Execution::Context& deviceList,
      QObject* parent);

  const Scenario::StateModel& iscoreState() const;

private:
  const Scenario::StateModel& m_iscore_state;
  ossia::time_event& m_root;

  const Engine::Execution::Context& m_context;
};
}
}

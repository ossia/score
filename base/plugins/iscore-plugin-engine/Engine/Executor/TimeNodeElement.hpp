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
class ISCORE_PLUGIN_ENGINE_EXPORT TimeNodeElement final : public QObject
{
public:
  TimeNodeElement(
      std::shared_ptr<ossia::time_node> ossia_tn,
      const Scenario::TimeNodeModel& element,
      const Device::DeviceList& devlist,
      QObject* parent);

  std::shared_ptr<ossia::time_node> OSSIATimeNode() const;
  const Scenario::TimeNodeModel& iscoreTimeNode() const;

private:
  std::shared_ptr<ossia::time_node> m_ossia_node;
  const Scenario::TimeNodeModel& m_iscore_node;

  const Device::DeviceList& m_deviceList;
};
}
}

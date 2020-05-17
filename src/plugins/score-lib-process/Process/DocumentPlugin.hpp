#pragma once
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <ossia/detail/ptr_set.hpp>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT DataflowManager final : public QObject
{
public:
  DataflowManager();
  ~DataflowManager();

  using cable_map = ossia::ptr_map<const Process::Cable*, Dataflow::CableItem*>;
  using port_map = ossia::ptr_map<const Process::Port*, Dataflow::PortItem*>;

  cable_map& cables() noexcept { return m_cableMap; }
  port_map& ports() noexcept { return m_portMap; }

private:
  cable_map m_cableMap;
  port_map m_portMap;
};
}

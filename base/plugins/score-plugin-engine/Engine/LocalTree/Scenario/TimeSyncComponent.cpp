// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncComponent.hpp"

#include "MetadataParameters.hpp"

#include <State/Value.hpp>

namespace Engine
{
namespace LocalTree
{
TimeSync::TimeSync(
    ossia::net::node_base& parent,
    const Id<score::Component>& id,
    Scenario::TimeSyncModel& timeSync,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, timeSync.metadata(), doc,
                      id,     "TimeSyncComponent", parent_comp}
{
  m_properties.push_back(add_setProperty<::State::impulse>(
      node(), "trigger", [&](auto) { timeSync.triggeredByGui(); }));
}
}
}

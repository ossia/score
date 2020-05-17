// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncComponent.hpp"

#include <State/Value.hpp>

#include <LocalTree/SetProperty.hpp>

namespace LocalTree
{
TimeSync::TimeSync(
    ossia::net::node_base& parent,
    const Id<score::Component>& id,
    Scenario::TimeSyncModel& timeSync,
    const score::DocumentContext& doc,
    QObject* parent_comp)
    : CommonComponent{parent, timeSync.metadata(), doc, id, "TimeSyncComponent", parent_comp}
{
  m_properties.push_back(add_setProperty<::State::impulse>(
      node(), "trigger", [t = QPointer<Scenario::TimeSyncModel>{&timeSync}](auto) {
        if (t)
          t->triggeredByGui();
      }));
}

TimeSync::~TimeSync() { }
}

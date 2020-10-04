#include "Sync.hpp"

namespace RemoteControl
{

Sync::Sync(
    const Id<score::Component>& id,
    Scenario::TimeSyncModel& timeSync,
    const DocumentPlugin& doc,
    QObject* parent_comp)
    : Component{id, "SyncComponent", parent_comp}
{
}

}

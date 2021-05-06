#include "Sync.hpp"

namespace RemoteControl
{

Sync::Sync(
    Scenario::TimeSyncModel& timeSync,
    const DocumentPlugin& doc,
    QObject* parent_comp)
    : Component{"SyncComponent", parent_comp}
{
}

}

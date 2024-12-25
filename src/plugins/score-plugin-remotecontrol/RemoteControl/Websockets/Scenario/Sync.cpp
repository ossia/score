#include "Sync.hpp"

namespace RemoteControl::WS
{

Sync::Sync(
    Scenario::TimeSyncModel& timeSync, const DocumentPlugin& doc, QObject* parent_comp)
    : Component{"SyncComponent", parent_comp}
{
}

}

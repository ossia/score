#include <Process/OfflineAction/OfflineAction.hpp>

namespace Process
{

OfflineAction::~OfflineAction() { }

OfflineActionList::OfflineActionList() { }

OfflineActionList::~OfflineActionList() { }

auto OfflineActionList::actionsForProcess(
    const UuidKey<ProcessModel>& key) const noexcept -> OfflineActions
{
  auto it = actionsMap.find(key);
  if(it != actionsMap.end())
  {
    return it->second;
  }
  return {};
}

}

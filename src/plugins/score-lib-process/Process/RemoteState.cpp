#include <Process/RemoteState.hpp>

namespace Process
{
std::vector<QPointer<ProcessModel>>& awaitingRemoteState() noexcept
{
  static std::vector<QPointer<ProcessModel>> pending;
  return pending;
}
}

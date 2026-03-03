#pragma once
#include <score/command/Command.hpp>

namespace ClipLauncher
{
class CellModel;

inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"ClipLauncher"};
  return key;
}
}

// Specialization for AddTrigger/RemoveTrigger templates
template <>
inline const CommandGroupKey& CommandFactoryName<ClipLauncher::CellModel>()
{
  return ClipLauncher::CommandFactoryName();
}

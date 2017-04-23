#pragma once
#include <iscore/model/path/Path.hpp>

namespace Process
{
class ProcessModel;
}
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class SlotModel;
class PutLayerModelToFront
{
public:
  PutLayerModelToFront(
      Path<SlotModel>&& slotPath, const Id<Process::ProcessModel>& pid);

  void redo() const;

private:
  Path<SlotModel> m_slotPath;
  const Id<Process::ProcessModel>& m_pid;
};
}

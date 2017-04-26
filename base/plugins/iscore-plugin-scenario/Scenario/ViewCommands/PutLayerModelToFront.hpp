#pragma once
#include <Scenario/Document/Constraint/Slot.hpp>

namespace Scenario
{
class PutLayerModelToFront
{
public:
  PutLayerModelToFront(
      SlotPath&& slotPath, const Id<Process::ProcessModel>& pid);

  void redo() const;

private:
  SlotPath m_slotPath;
  const Id<Process::ProcessModel>& m_pid;
};
}

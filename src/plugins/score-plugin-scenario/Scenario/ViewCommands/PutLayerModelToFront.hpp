#pragma once
#include <Scenario/Document/Interval/Slot.hpp>

namespace Scenario
{
class PutLayerModelToFront
{
public:
  PutLayerModelToFront(SlotPath&& slotPath, const Id<Process::ProcessModel>& pid);

  void redo(const score::DocumentContext& ctx) const;

private:
  SlotPath m_slotPath;
  const Id<Process::ProcessModel>& m_pid;
};
}

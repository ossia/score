#pragma once
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

namespace Scenario
{
class PutLayerModelToFront
{
public:
  PutLayerModelToFront(
      SlotIdentifier&& slotPath, const Id<Process::ProcessModel>& pid);

  void redo() const;

private:
  SlotIdentifier m_slotPath;
  const Id<Process::ProcessModel>& m_pid;
};
}

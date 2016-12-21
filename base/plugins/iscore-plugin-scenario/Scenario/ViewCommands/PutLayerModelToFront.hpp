#pragma once
#include <iscore/model/path/Path.hpp>

namespace Process
{
class LayerModel;
}
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class SlotModel;
class PutLayerModelToFront
{
public:
  PutLayerModelToFront(
      Path<SlotModel>&& slotPath, const Id<Process::LayerModel>& pid);

  void redo() const;

private:
  Path<SlotModel> m_slotPath;
  const Id<Process::LayerModel>& m_pid;
};
}

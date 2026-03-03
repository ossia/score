#include "AddScene.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/SceneModel.hpp>

namespace ClipLauncher
{

AddScene::AddScene(const ProcessModel& model, int position)
    : m_path{model}
    , m_sceneId{getStrongId(model.scenes)}
    , m_position{position}
{
  // Pre-generate cell IDs for each lane
  m_cellIds = getStrongIdRange<CellModel>(model.laneCount(), model.cells);
}

void AddScene::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  // Remove cells first
  for(auto& cellId : m_cellIds)
    proc.cells.remove(cellId);
  proc.scenes.remove(m_sceneId);
}

void AddScene::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);
  auto scene = new SceneModel{m_sceneId, &proc};
  scene->setName(QString("Scene %1").arg(m_position + 1));
  proc.scenes.add(scene);

  // Auto-create cells for every lane
  int laneIdx = 0;
  for(auto& cellId : m_cellIds)
  {
    proc.cells.add(ProcessModel::createDefaultCell(
        cellId, laneIdx, m_position, proc.context(), &proc));
    laneIdx++;
  }
}

void AddScene::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_sceneId << m_cellIds << m_position;
}

void AddScene::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_sceneId >> m_cellIds >> m_position;
}

} // namespace ClipLauncher

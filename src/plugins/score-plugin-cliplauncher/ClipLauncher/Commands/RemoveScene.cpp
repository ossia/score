#include "RemoveScene.hpp"

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/SceneModel.hpp>

namespace ClipLauncher
{

RemoveScene::RemoveScene(const ProcessModel& model, const SceneModel& scene)
    : m_path{model}
    , m_sceneId{scene.id()}
    , m_sceneData{score::marshall<DataStream>(scene)}
{
  for(auto& cell : model.cells)
  {
    if(cell.scene() == scene.id().val())
    {
      m_cellIds.push_back(cell.id());
      m_cellData.push_back(score::marshall<DataStream>(cell));
    }
  }
}

void RemoveScene::undo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);

  DataStream::Deserializer s{m_sceneData};
  auto scene = new SceneModel{s, &proc};
  proc.scenes.add(scene);

  for(const auto& data : m_cellData)
  {
    DataStream::Deserializer cs{data};
    auto cell = new CellModel{cs, proc.context(), &proc};
    proc.cells.add(cell);
  }
}

void RemoveScene::redo(const score::DocumentContext& ctx) const
{
  auto& proc = m_path.find(ctx);

  for(const auto& cellId : m_cellIds)
    proc.cells.remove(cellId);

  proc.scenes.remove(m_sceneId);
}

void RemoveScene::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_sceneId << m_sceneData << m_cellData << m_cellIds;
}

void RemoveScene::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_sceneId >> m_sceneData >> m_cellData >> m_cellIds;
}

} // namespace ClipLauncher

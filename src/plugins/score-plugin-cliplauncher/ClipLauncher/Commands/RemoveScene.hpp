#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;
class SceneModel;

class RemoveScene final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveScene, "Remove a scene")
public:
  RemoveScene(const ProcessModel& model, const SceneModel& scene);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<SceneModel> m_sceneId;
  QByteArray m_sceneData;
  std::vector<QByteArray> m_cellData;
  std::vector<Id<CellModel>> m_cellIds;
};

} // namespace ClipLauncher

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

class AddScene final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddScene, "Add a scene")
public:
  AddScene(const ProcessModel& model, int position);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<SceneModel> m_sceneId;
  std::vector<Id<CellModel>> m_cellIds;
  int m_position{};
};

} // namespace ClipLauncher

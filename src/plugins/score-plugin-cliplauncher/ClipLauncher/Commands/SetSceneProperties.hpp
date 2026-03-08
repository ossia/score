#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class SceneModel;
class ProcessModel;

class SetSceneName final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetSceneName, "Set scene name")
public:
  SetSceneName(const ProcessModel& proc, const SceneModel& scene, QString newName);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<SceneModel> m_sceneId;
  QString m_old, m_new;
};

} // namespace ClipLauncher

#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class MoveCell final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveCell, "Move a cell")
public:
  MoveCell(const ProcessModel& model, const CellModel& cell, int newLane, int newScene);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  int m_oldLane{};
  int m_oldScene{};
  int m_newLane{};
  int m_newScene{};
};

} // namespace ClipLauncher

#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class RemoveCell final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveCell, "Remove a cell")
public:
  RemoveCell(const ProcessModel& model, const CellModel& cell);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  QByteArray m_cellData;
};

} // namespace ClipLauncher

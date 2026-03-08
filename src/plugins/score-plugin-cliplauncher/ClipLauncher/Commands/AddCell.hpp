#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class AddCell final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddCell, "Add a cell")
public:
  AddCell(const ProcessModel& model, int lane, int scene);

  const Id<CellModel>& cellId() const noexcept { return m_cellId; }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  int m_lane{};
  int m_scene{};
};

} // namespace ClipLauncher

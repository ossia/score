#pragma once
#include <ClipLauncher/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;
class LaneModel;

class RemoveLane final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveLane, "Remove a lane")
public:
  RemoveLane(const ProcessModel& model, const LaneModel& lane);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  QByteArray m_laneData;
  std::vector<QByteArray> m_cellData; // Cells in this lane, for undo
  std::vector<Id<CellModel>> m_cellIds;
};

} // namespace ClipLauncher

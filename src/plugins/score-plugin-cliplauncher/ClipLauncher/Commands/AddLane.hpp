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

class AddLane final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddLane, "Add a lane")
public:
  AddLane(const ProcessModel& model, int position);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  std::vector<Id<CellModel>> m_cellIds;
  int m_position{};
};

} // namespace ClipLauncher

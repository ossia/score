#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/NodeLayout.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <QPointF>

namespace Process
{
class ProcessModel;
}
namespace Scenario::Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT LayoutNodes final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), LayoutNodes, "Lay out nodes")

public:
  explicit LayoutNodes(const std::vector<NodePlacement>& placements);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  std::vector<std::pair<Path<Process::ProcessModel>, QPointF>> m_before;
  std::vector<QPointF> m_after;
};
}

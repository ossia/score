#pragma once
#include <ClipLauncher/CommandFactory.hpp>
#include <ClipLauncher/TransitionRule.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class RemoveTransitionRule final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveTransitionRule, "Remove transition rule")
public:
  RemoveTransitionRule(
      const ProcessModel& proc, const CellModel& cell, TransitionRule rule);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  TransitionRule m_savedRule;
};

} // namespace ClipLauncher

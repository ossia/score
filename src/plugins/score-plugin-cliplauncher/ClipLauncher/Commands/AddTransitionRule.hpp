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

class AddTransitionRule final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddTransitionRule, "Add transition rule")
public:
  AddTransitionRule(
      const ProcessModel& proc, const CellModel& cell, TransitionRule rule);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  TransitionRule m_rule;
};

} // namespace ClipLauncher

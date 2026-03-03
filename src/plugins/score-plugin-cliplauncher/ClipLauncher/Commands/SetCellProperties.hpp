#pragma once
#include <ClipLauncher/CommandFactory.hpp>
#include <ClipLauncher/Types.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class SetCellLaunchMode final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetCellLaunchMode, "Set cell launch mode")
public:
  SetCellLaunchMode(const ProcessModel& proc, const CellModel& cell, LaunchMode newMode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  LaunchMode m_old, m_new;
};

class SetCellTriggerStyle final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetCellTriggerStyle, "Set cell trigger style")
public:
  SetCellTriggerStyle(
      const ProcessModel& proc, const CellModel& cell, TriggerStyle newStyle);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  TriggerStyle m_old, m_new;
};

class SetCellVelocity final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetCellVelocity, "Set cell velocity")
public:
  SetCellVelocity(const ProcessModel& proc, const CellModel& cell, double newVel);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<CellModel> m_cellId;
  double m_old, m_new;
};

} // namespace ClipLauncher

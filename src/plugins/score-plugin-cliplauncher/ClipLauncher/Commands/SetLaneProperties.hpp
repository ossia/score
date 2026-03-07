#pragma once
#include <ClipLauncher/CommandFactory.hpp>
#include <ClipLauncher/Types.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace ClipLauncher
{
class LaneModel;
class ProcessModel;

class SetLaneName final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetLaneName, "Set lane name")
public:
  SetLaneName(const ProcessModel& proc, const LaneModel& lane, QString newName);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  QString m_old, m_new;
};

class SetLaneExclusivityMode final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(), SetLaneExclusivityMode, "Set lane exclusivity mode")
public:
  SetLaneExclusivityMode(
      const ProcessModel& proc, const LaneModel& lane, ExclusivityMode newMode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  ExclusivityMode m_old, m_new;
};

class SetLaneVolume final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetLaneVolume, "Set lane volume")
public:
  SetLaneVolume(const ProcessModel& proc, const LaneModel& lane, double newVolume);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  double m_old, m_new;
};

class SetLaneBlendMode final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetLaneBlendMode, "Set lane blend mode")
public:
  SetLaneBlendMode(
      const ProcessModel& proc, const LaneModel& lane, VideoBlendMode newMode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  VideoBlendMode m_old, m_new;
};

class SetLaneVideoOpacity final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetLaneVideoOpacity, "Set lane video opacity")
public:
  SetLaneVideoOpacity(const ProcessModel& proc, const LaneModel& lane, double newOpacity);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  Id<LaneModel> m_laneId;
  double m_old, m_new;
};

} // namespace ClipLauncher

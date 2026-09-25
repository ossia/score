#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Unused.hpp>

PROPERTY_COMMAND_T(Process, SetLoop, ProcessModel::p_loops, "Set process looping")
SCORE_COMMAND_DECL_T(Process::SetLoop)
PROPERTY_COMMAND_T(
    Process, SetProcessScriptable, ProcessModel::p_scriptable, "Set process scriptable")
SCORE_COMMAND_DECL_T(Process::SetProcessScriptable)
PROPERTY_COMMAND_T(
    Process, SetStartOffset, ProcessModel::p_startOffset, "Set start offset")
SCORE_COMMAND_DECL_T(Process::SetStartOffset)
PROPERTY_COMMAND_T(
    Process, SetLoopDuration, ProcessModel::p_loopDuration, "Set loop duration")
SCORE_COMMAND_DECL_T(Process::SetLoopDuration)

PROPERTY_COMMAND_T(Process, MoveNode, ProcessModel::p_position, "Move node")
SCORE_COMMAND_DECL_T(Process::MoveNode)
PROPERTY_COMMAND_T(Process, ResizeNode, ProcessModel::p_size, "Resize node")
SCORE_COMMAND_DECL_T(Process::ResizeNode)
PROPERTY_COMMAND_T(Process, SetNodeFoldMode, ProcessModel::p_foldMode, "Fold node")
SCORE_COMMAND_DECL_T(Process::SetNodeFoldMode)

namespace Process
{
//! p_duration is read-only and typed in flicks, so this cannot be a
//! PROPERTY_COMMAND_T. The expand mode is a parameter rather than read from the
//! edition settings, which live above this library: the caller supplies it, so
//! the Scale / Lock toolbar toggle governs whether content is rescaled.
class SCORE_LIB_PROCESS_EXPORT SetDuration final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), SetDuration, "Set process duration")
public:
  SetDuration(const ProcessModel& proc, TimeVal newDuration, ExpandMode mode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(unused_t, TimeVal newDuration, ExpandMode mode);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  TimeVal m_old{}, m_new{};
  ExpandMode m_mode{};
};

class MoveNodes final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), MoveNodes, "Move nodes")
public:
  MoveNodes(std::vector<const ProcessModel*> processes, QPointF delta);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(unused_t, QPointF delta);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  std::vector<std::pair<Path<ProcessModel>, QPointF>> m_models;
  QPointF m_delta;
};

class MoveNodesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveNodesMacro, "Move nodes")
};

//! Control values written during a playback run with "Record playback" on.
class SCORE_LIB_PROCESS_EXPORT RecordPlayback final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RecordPlayback, "Record playback")
};

//! Control values written during a playback run without recording, kept on request.
class SCORE_LIB_PROCESS_EXPORT KeepPlayedValues final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), KeepPlayedValues, "Keep played values")
};

//! Control values set by playing a state while stopped.
class SCORE_LIB_PROCESS_EXPORT RecallState final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RecallState, "Play state")
};

class SCORE_LIB_PROCESS_EXPORT RenameProcess final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), RenameProcess, "Rename process")
public:
  RenameProcess(const ProcessModel& process, QString name);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  QString m_old, m_new;
};
}

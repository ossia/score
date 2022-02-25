#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <score/command/PropertyCommand.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Unused.hpp>

PROPERTY_COMMAND_T(
    Process,
    SetLoop,
    ProcessModel::p_loops,
    "Set process looping")
SCORE_COMMAND_DECL_T(Process::SetLoop)
PROPERTY_COMMAND_T(
    Process,
    SetStartOffset,
    ProcessModel::p_startOffset,
    "Set start offset")
SCORE_COMMAND_DECL_T(Process::SetStartOffset)
PROPERTY_COMMAND_T(
    Process,
    SetLoopDuration,
    ProcessModel::p_loopDuration,
    "Set loop duration")
SCORE_COMMAND_DECL_T(Process::SetLoopDuration)

PROPERTY_COMMAND_T(Process, MoveNode, ProcessModel::p_position, "Move node")
SCORE_COMMAND_DECL_T(Process::MoveNode)
PROPERTY_COMMAND_T(Process, ResizeNode, ProcessModel::p_size, "Resize node")
SCORE_COMMAND_DECL_T(Process::ResizeNode)

namespace Process
{
class MoveNodes final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), MoveNodes, "Move nodes")
public:
  MoveNodes(
      std::vector<const ProcessModel*> processes,
      QPointF delta);

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

}

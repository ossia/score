#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <Process/Process.hpp>
namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT LoadPreset final : public score::Command
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(),
      LoadPreset,
      "Set a control")

public:
  LoadPreset(const Process::ProcessModel& obj, Process::Preset newval)
      : m_path{obj}, m_old{obj.savePreset()}, m_new{std::move(newval)}
  {
  }

  virtual ~LoadPreset() {}

private:
  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).loadPreset(m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).loadPreset(m_new);
  }

  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_old >> m_new;
  }

  Path<Process::ProcessModel> m_path;
  Process::Preset m_old;
  Process::Preset m_new;
};
}

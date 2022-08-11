#pragma once
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT SetControlValue final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), SetControlValue, "Set a control")

public:
  SetControlValue(const Process::ControlInlet& obj, ossia::value newval);
  virtual ~SetControlValue();

  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;
  void update(const Process::ControlInlet& obj, ossia::value newval);

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Process::ControlInlet> m_path;
  ossia::value m_old, m_new;
};

class SCORE_LIB_PROCESS_EXPORT SetControlOutletValue final : public score::Command
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), SetControlOutletValue, "Set a control")

public:
  SetControlOutletValue(const Process::ControlOutlet& obj, ossia::value newval);
  virtual ~SetControlOutletValue();

  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;
  void update(const Process::ControlOutlet& obj, ossia::value newval);

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Process::ControlOutlet> m_path;
  ossia::value m_old, m_new;
};
}

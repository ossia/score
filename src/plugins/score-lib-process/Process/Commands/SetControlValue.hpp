#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT SetControlValue final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), SetControlValue, "Set a control")

public:
  SetControlValue(const Process::ControlInlet& obj, ossia::value newval)
      : m_path{obj}, m_old{obj.value()}, m_new{newval}
  {
  }

  virtual ~SetControlValue() { }

  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setValue(m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setValue(m_new);
  }

  void update(const Process::ControlInlet& obj, ossia::value newval) { m_new = std::move(newval); }

protected:
  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_old >> m_new;
  }

private:
  Path<Process::ControlInlet> m_path;
  ossia::value m_old, m_new;
};

class SCORE_LIB_PROCESS_EXPORT SetControlOutletValue final : public score::Command
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), SetControlOutletValue, "Set a control")

public:
  SetControlOutletValue(const Process::ControlOutlet& obj, ossia::value newval)
      : m_path{obj}, m_old{obj.value()}, m_new{newval}
  {
  }

  virtual ~SetControlOutletValue() { }

  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setValue(m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setValue(m_new);
  }

  void update(const Process::ControlOutlet& obj, ossia::value newval)
  {
    m_new = std::move(newval);
  }

protected:
  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_old >> m_new;
  }

private:
  Path<Process::ControlOutlet> m_path;
  ossia::value m_old, m_new;
};
}

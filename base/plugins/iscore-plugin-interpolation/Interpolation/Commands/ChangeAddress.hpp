#pragma once
#include <Interpolation/Commands/CommandFactory.hpp>
#include <Interpolation/InterpolationProcess.hpp>

#include <iscore/command/PropertyCommand.hpp>
#include <iscore_plugin_interpolation_export.h>

namespace Interpolation
{
class ProcessModel;
class ISCORE_PLUGIN_INTERPOLATION_EXPORT ChangeAddress final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      CommandFactoryName(), ChangeAddress, "Change Interpolation Address")
public:
  ChangeAddress(
      const ProcessModel& proc,
      const State::AddressAccessor& addr,
      const State::Value& start,
      const State::Value& end,
      const State::Unit& u);

public:
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  State::AddressAccessor m_oldAddr, m_newAddr;
  State::Unit m_oldUnit, m_newUnit;
  State::Value m_oldStart, m_newStart;
  State::Value m_oldEnd, m_newEnd;
};

// MOVEME && should apply to both Interpolation and Automation
class ISCORE_PLUGIN_INTERPOLATION_EXPORT SetTween final
    : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set curve tween")
public:
  SetTween(Path<ProcessModel>&& path, bool newval)
      : iscore::PropertyCommand{std::move(path), "tween", newval}
  {
  }
};
}

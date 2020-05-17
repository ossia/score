#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>

#include <Interpolation/InterpolationProcess.hpp>

namespace Interpolation
{
class ProcessModel;
class ChangeAddress final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(),
      ChangeAddress,
      "Change Interpolation Address")
public:
  ChangeAddress(
      const ProcessModel& proc,
      const State::AddressAccessor& addr,
      const ossia::value& start,
      const ossia::value& end,
      const State::Unit& u);

public:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  State::AddressAccessor m_oldAddr, m_newAddr;
  State::Unit m_oldUnit, m_newUnit;
  ossia::value m_oldStart, m_newStart;
  ossia::value m_oldEnd, m_newEnd;
};

void ChangeInterpolationAddress(
    const Interpolation::ProcessModel& proc,
    const ::State::AddressAccessor& addr,
    CommandDispatcher<>& disp);

// MOVEME && should apply to both Interpolation and Automation
class SetTween final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Scenario::Command::CommandFactoryName(), SetTween, "Set interpolation tween")
public:
  SetTween(const ProcessModel& path, bool newval)
      : score::PropertyCommand{std::move(path), "tween", newval}
  {
  }
};
}

#pragma once
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/MapSerialization.hpp>

namespace Scenario
{
namespace Command
{
class SetTimeSignatures final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetTimeSignatures, "Set time signatures")
public:
  SetTimeSignatures(const IntervalModel& cst, TimeSignatureMap newval)
      : m_path{cst}, m_oldVal{cst.timeSignatureMap()}, m_newVal{std::move(newval)}
  {
  }

  void update(const IntervalModel& cst, TimeSignatureMap newval) { m_newVal = std::move(newval); }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setTimeSignatureMap(m_oldVal);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).setTimeSignatureMap(m_newVal);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override { s << m_path << m_oldVal << m_newVal; }
  void deserializeImpl(DataStreamOutput& s) override { s >> m_path >> m_oldVal >> m_newVal; }

private:
  Path<IntervalModel> m_path;

  TimeSignatureMap m_oldVal;
  TimeSignatureMap m_newVal;
};

using IntervalModel = ::Scenario::IntervalModel;
}
}

PROPERTY_COMMAND_T(
    Scenario::Command,
    SetHasTimeSignature,
    IntervalModel::p_timeSignature,
    "Change time signature")
SCORE_COMMAND_DECL_T(Scenario::Command::SetHasTimeSignature)

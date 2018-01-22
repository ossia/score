#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <tests/helpers/ForwardDeclaration.hpp>
#include <Process/TimeValueSerialization.hpp>

namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
 * @brief The SetMaxDuration class
 *
 * Sets the Max duration of a Interval
*/
class SetMaxDuration final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetMaxDuration, "Set interval maximum")
public:
  static const constexpr auto corresponding_member
      = &IntervalDurations::maxDuration; // used by state machine
                                           // (MoveState.hpp)

  SetMaxDuration(const IntervalModel& cst, TimeVal newval, bool isInfinite)
      : m_path{cst}
      , m_oldVal{cst.duration.maxDuration()}
      , m_newVal{std::move(newval)}
      , m_newInfinite{isInfinite}
      , m_oldInfinite{cst.duration.isMaxInfinite()}
  {
  }

  void
  update(const IntervalModel& cst, const TimeVal& newval, bool isInfinite)
  {
    m_newVal = newval;
    auto& cstrDuration = cst.duration;
    if (m_newVal < cstrDuration.defaultDuration())
      m_newVal = cstrDuration.defaultDuration();
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).duration.setMaxInfinite(m_oldInfinite);
    m_path.find(ctx).duration.setMaxDuration(m_oldVal);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_path.find(ctx).duration.setMaxInfinite(m_newInfinite);
    m_path.find(ctx).duration.setMaxDuration(m_newVal);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldVal << m_newVal << m_newInfinite;
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldVal >> m_newVal >> m_newInfinite;
  }

private:
  Path<IntervalModel> m_path;

  TimeVal m_oldVal;
  TimeVal m_newVal;
  bool m_newInfinite{};
  bool m_oldInfinite{};
};
}
}

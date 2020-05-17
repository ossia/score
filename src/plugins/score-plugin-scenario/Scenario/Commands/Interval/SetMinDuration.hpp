#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
 * @brief The SetMinDuration class
 *
 * Sets the Min duration of a Interval
 */
class SetMinDuration final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetMinDuration, "Set interval minimum")
public:
  static const constexpr auto corresponding_member = &IntervalDurations::minDuration;

  SetMinDuration(const IntervalModel& cst, TimeVal newval, bool isMinNull)
      : m_path{cst}
      , m_oldVal{cst.duration.minDuration()}
      , m_newVal{newval}
      , m_oldMinNull{cst.duration.isMinNull()}
      , m_newMinNull{isMinNull}
  {
  }

  void update(const IntervalModel& cst, TimeVal newval, bool isMinNull)
  {
    m_newVal = newval;
    auto& cstrDuration = cst.duration;
    if (m_newVal < TimeVal::zero())
      m_newVal = TimeVal::zero();
    if (m_newVal > cstrDuration.defaultDuration())
      m_newVal = cstrDuration.defaultDuration();
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& cstrDuration = m_path.find(ctx).duration;
    cstrDuration.setMinNull(m_oldMinNull);
    cstrDuration.setMinDuration(m_oldVal);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& cstrDuration = m_path.find(ctx).duration;
    cstrDuration.setMinNull(m_newMinNull);
    cstrDuration.setMinDuration(m_newVal);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldVal << m_newVal << m_oldMinNull << m_newMinNull;
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldVal >> m_newVal >> m_oldMinNull >> m_newMinNull;
  }

private:
  Path<IntervalModel> m_path;

  TimeVal m_oldVal;
  TimeVal m_newVal;
  bool m_oldMinNull{}, m_newMinNull{};
};
}
}

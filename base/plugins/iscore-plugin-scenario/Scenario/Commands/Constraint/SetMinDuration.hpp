#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <tests/helpers/ForwardDeclaration.hpp>

namespace Scenario
{
class ConstraintModel;
namespace Command
{
/**
 * @brief The SetMinDuration class
 *
 * Sets the Min duration of a Constraint
 */
class SetMinDuration final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetMinDuration, "Set constraint minimum")
public:
  static const constexpr auto corresponding_member
      = &ConstraintDurations::minDuration;

  SetMinDuration(const ConstraintModel& cst, TimeVal newval, bool isMinNull)
      : m_path{cst}
      , m_oldVal{cst.duration.minDuration()}
      , m_newVal{newval}
      , m_oldMinNull{cst.duration.isMinNull()}
      , m_newMinNull{isMinNull}
  {
  }

  void update(const ConstraintModel& cst, TimeVal newval, bool isMinNull)
  {
    m_newVal = newval;
    auto& cstrDuration = cst.duration;
    if (m_newVal < TimeVal::zero())
      m_newVal = TimeVal::zero();
    if (m_newVal > cstrDuration.defaultDuration())
      m_newVal = cstrDuration.defaultDuration();
  }

  void undo() const override
  {
    auto& cstrDuration = m_path.find().duration;
    cstrDuration.setMinNull(m_oldMinNull);
    cstrDuration.setMinDuration(m_oldVal);
  }

  void redo() const override
  {
    auto& cstrDuration = m_path.find().duration;
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
  Path<ConstraintModel> m_path;

  TimeVal m_oldVal;
  TimeVal m_newVal;
  bool m_oldMinNull, m_newMinNull;
};
}
}

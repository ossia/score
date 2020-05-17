#pragma once
#include <Midi/Commands/CommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <Patternist/PatternModel.hpp>
namespace Patternist
{
inline auto& CommandFactoryName()
{
  return Midi::CommandFactoryName();
}
class UpdatePattern final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), UpdatePattern, "Update a pattern")
public:
  UpdatePattern(const ProcessModel& model, int p, const Pattern& pat)
      : m_model{model}, m_id{p}, m_old{model.patterns()[p]}, m_new{pat}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPattern(m_id, m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPattern(m_id, m_new);
  }

  void update(const ProcessModel& model, int p, const Pattern& pat) { m_new = pat; }

protected:
  void serializeImpl(DataStreamInput& s) const override { s << m_model << m_id << m_old << m_new; }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_model >> m_id >> m_old >> m_new; }

private:
  Path<ProcessModel> m_model;
  int m_id{};
  Pattern m_old;
  Pattern m_new;
};

}
PROPERTY_COMMAND_T(Patternist, SetPatternChannel, ProcessModel::p_channel, "Change channel")
SCORE_COMMAND_DECL_T(Patternist::SetPatternChannel)
PROPERTY_COMMAND_T(Patternist, SetCurrentPattern, ProcessModel::p_currentPattern, "Change pattern")
SCORE_COMMAND_DECL_T(Patternist::SetCurrentPattern)

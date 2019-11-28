#pragma once
#include <score/command/Command.hpp>
#include <Patternist/PatternCommandFactory.hpp>
#include <Patternist/PatternModel.hpp>
#include <score/model/path/PathSerialization.hpp>
namespace Patternist
{
class UpdatePattern final : public score::Command
{
  SCORE_COMMAND_DECL(Patternist::CommandFactoryName(), UpdatePattern, "Update a pattern")
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

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_id << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_model >> m_id >> m_old >> m_new;
  }

private:
  Path<ProcessModel> m_model;
  int m_id{};
  Pattern m_old;
  Pattern m_new;
};

}

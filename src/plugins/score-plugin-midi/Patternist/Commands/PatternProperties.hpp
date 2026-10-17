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
//! Edits patterns, one undo step. Ongoing, it follows the edit from pattern to
//! pattern: the shown one can change in the middle of a drag.
class UpdatePattern final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), UpdatePattern, "Update a pattern")
public:
  UpdatePattern(const ProcessModel& model, int p, const Pattern& pat)
      : m_model{model}
      , m_old{model.patterns()}
      , m_new{model.patterns()}
  {
    update(model, p, pat);
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPatterns(m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPatterns(m_new);
  }

  void update(const ProcessModel& model, int p, const Pattern& pat)
  {
    if(ossia::valid_index(p, m_new))
      m_new[p] = pat;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_model >> m_old >> m_new;
  }

private:
  Path<ProcessModel> m_model;
  std::vector<Pattern> m_old;
  std::vector<Pattern> m_new;
};

class UpdatePatterns final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), UpdatePatterns, "Update patterns")
public:
  UpdatePatterns(const ProcessModel& model, std::vector<Pattern> pat)
      : m_model{model}
      , m_old{model.patterns()}
      , m_new{std::move(pat)}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPatterns(m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setPatterns(m_new);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_model >> m_old >> m_new; }

private:
  Path<ProcessModel> m_model;
  std::vector<Pattern> m_old;
  std::vector<Pattern> m_new;
};

//! Changes the pattern list and which one is current together, so that adding
//! or removing a pattern is a single undo step.
class ReplacePatterns final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), ReplacePatterns, "Change the patterns")
public:
  ReplacePatterns(const ProcessModel& model, std::vector<Pattern> pat, int current)
      : m_model{model}
      , m_old{model.patterns()}
      , m_new{std::move(pat)}
      , m_oldCurrent{model.currentPattern()}
      , m_newCurrent{current}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& m = m_model.find(ctx);
    m.setPatterns(m_old);
    m.setCurrentPattern(m_oldCurrent);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& m = m_model.find(ctx);
    m.setPatterns(m_new);
    m.setCurrentPattern(m_newCurrent);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_old << m_new << m_oldCurrent << m_newCurrent;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_model >> m_old >> m_new >> m_oldCurrent >> m_newCurrent;
  }

private:
  Path<ProcessModel> m_model;
  std::vector<Pattern> m_old;
  std::vector<Pattern> m_new;
  int m_oldCurrent{};
  int m_newCurrent{};
};
}
PROPERTY_COMMAND_T(
    Patternist, SetPatternChannel, ProcessModel::p_channel, "Change channel")
SCORE_COMMAND_DECL_T(Patternist::SetPatternChannel)
PROPERTY_COMMAND_T(
    Patternist, SetCurrentPattern, ProcessModel::p_currentPattern, "Change pattern")
SCORE_COMMAND_DECL_T(Patternist::SetCurrentPattern)
PROPERTY_COMMAND_T(
    Patternist, SetLaneLabels, ProcessModel::p_laneLabels, "Change lane labels")
SCORE_COMMAND_DECL_T(Patternist::SetLaneLabels)

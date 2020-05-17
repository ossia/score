#pragma once
#include <Midi/Commands/CommandFactory.hpp>

#include <score/model/path/Path.hpp>

namespace Midi
{
class ProcessModel;

class SetChannel final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), SetChannel, "Set Midi channel")
public:
  // Channel should be in [ 0; 15 ]
  SetChannel(const ProcessModel& model, int chan);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  int m_old{}, m_new{};
};

class SetRange final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), SetRange, "Set Midi range")
public:
  // Channel should be in [ 0; 15 ]
  SetRange(const ProcessModel& model, int min, int max);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  int m_oldmin{}, m_newmin{};
  int m_oldmax{}, m_newmax{};
};
}

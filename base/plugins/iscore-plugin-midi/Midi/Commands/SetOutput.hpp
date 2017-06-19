#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <iscore/model/path/Path.hpp>

namespace Midi
{
class ProcessModel;

class SetOutput final : public iscore::Command
{
  ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), SetOutput, "Set Midi output")
public:
  SetOutput(const ProcessModel& model, const QString& dev);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  QString m_old, m_new;
};

class SetChannel final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      Midi::CommandFactoryName(), SetChannel, "Set Midi channel")
public:
  // Channel should be in [ 0; 15 ]
  SetChannel(const ProcessModel& model, int chan);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  int m_old{}, m_new{};
};
}

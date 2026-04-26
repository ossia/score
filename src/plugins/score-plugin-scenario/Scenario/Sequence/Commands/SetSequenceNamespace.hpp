#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <State/Address.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Sequence
{
namespace Command
{

// Adds one address to the sequence namespace (creates flat automations in every section).
class AddSequenceParameter final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), AddSequenceParameter,
      "Add sequence parameter")
public:
  AddSequenceParameter(const SequenceModel& seq, State::AddressAccessor addr);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  State::AddressAccessor m_addr;
};

// Removes one address from the sequence namespace (removes automations from every section).
class RemoveSequenceParameter final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), RemoveSequenceParameter,
      "Remove sequence parameter")
public:
  RemoveSequenceParameter(const SequenceModel& seq, State::AddressAccessor addr);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  State::AddressAccessor m_addr;
};

}
}

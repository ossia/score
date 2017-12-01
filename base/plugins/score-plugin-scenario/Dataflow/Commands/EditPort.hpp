#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Process/Process.hpp>
#include <score/model/path/Path.hpp>
#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <State/Address.hpp>

namespace Dataflow
{

class ChangePortAddress final : public score::Command
{
    SCORE_COMMAND_DECL(
        Scenario::Command::ScenarioCommandFactoryName(),
        ChangePortAddress,
        "Edit a node port")
    public:
      ChangePortAddress(
        const Process::Port& p,
        State::AddressAccessor addr);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

  protected:
    void serializeImpl(DataStreamInput& s) const override;
    void deserializeImpl(DataStreamOutput& s) override;

  private:
    Path<Process::Port> m_model;

    State::AddressAccessor m_old, m_new;
};


class SetPortPropagate final : public score::PropertyCommand
{
    SCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), SetPortPropagate, "Set port propagation")
    public:
      SetPortPropagate(
        const Process::Port& p,
        bool newval)
    : score::PropertyCommand{p, "propagate", newval}
{
}
};

}

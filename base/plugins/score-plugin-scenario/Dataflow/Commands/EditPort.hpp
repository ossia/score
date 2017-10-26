#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Process/Process.hpp>
#include <Dataflow/UI/DataflowProcessNode.hpp>
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

/*
class AddPort final : public score::Command
{
    SCORE _COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), AddPort, "Add a node port")

    public:
        AddPort(const Process::DataflowProcess& model, bool inlet);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

protected:
    void serializeImpl(DataStreamInput& s) const override;
    void deserializeImpl(DataStreamOutput& s) override;

private:
    Path<Process::DataflowProcess> m_model;
    bool m_inlet{}; // true : inlet ; false : outlet
};

class EditPort final : public score::Command
{
    SCORE _COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), EditPort, "Edit a node port")
    public:
        EditPort(const Process::DataflowProcess& model,
                 Process::Port next,
                 std::size_t index, bool inlet);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

protected:
    void serializeImpl(DataStreamInput& s) const override;
    void deserializeImpl(DataStreamOutput& s) override;

private:
    Path<Process::DataflowProcess> m_model;

    Process::Port m_old, m_new;
    quint64 m_index{};
    bool m_inlet{}; // true : inlet ; false : outlet
};

class RemovePort final : public score::Command
{
    SCORE _COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), RemovePort, "Remove a node port")

    public:
        RemovePort(const Process::DataflowProcess& model,
                   std::size_t index, bool inlet);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

protected:
    void serializeImpl(DataStreamInput& s) const override;
    void deserializeImpl(DataStreamOutput& s) override;

private:
    Path<Process::DataflowProcess> m_model;
    Process::Port m_old;
    quint64 m_index{};
    bool m_inlet{}; // true : inlet ; false : outlet
};
*/
}

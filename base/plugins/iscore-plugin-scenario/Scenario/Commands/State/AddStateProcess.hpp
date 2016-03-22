#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>
namespace Process
{
class StateProcess;
class StateProcessFactory;
}
namespace Scenario
{
class StateModel;
namespace Command
{
// MOVEME
class ISCORE_PLUGIN_SCENARIO_EXPORT AddStateProcessToState final :
        public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddStateProcessToState, "Add a state process")
    public:
        AddStateProcessToState(
            Path<StateModel>&& state,
            const UuidKey<Process::StateProcessFactory>& process);
        AddStateProcessToState(
            Path<StateModel>&& state,
            const Id<Process::StateProcess>& idToUse,
            const UuidKey<Process::StateProcessFactory>& process);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<StateModel> m_path;
        UuidKey<Process::StateProcessFactory> m_processName;

        Id<Process::StateProcess> m_createdProcessId {};
};


}
}

#pragma once
#include <Process/TimeValue.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include "CreateEvent_State.hpp"
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class ScenarioModel;
class EventModel;
class StateModel;
class TimeNodeModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateTimeNode_Event_State final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateTimeNode_Event_State,"Create a timenode, an event and a state")
        public:

          CreateTimeNode_Event_State(
            const Scenario::ScenarioModel& scenario,
            TimeValue date,
            double stateY);

        CreateTimeNode_Event_State(
          const Path<Scenario::ScenarioModel>& scenario,
          TimeValue date,
          double stateY);

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_command.scenarioPath(); }

        const Id<StateModel>& createdState() const
        { return m_command.createdState(); }

        const Id<EventModel>& createdEvent() const
        { return m_command.createdEvent(); }

        const Id<TimeNodeModel>& createdTimeNode() const
        { return m_newTimeNode; }

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Id<TimeNodeModel> m_newTimeNode;
        TimeValue m_date;

        CreateEvent_State m_command;
};
}
}

#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

class EventModel;
class ScenarioModel;

class SerializableMoveEvent : public iscore::SerializableCommand
{
ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SerializableMoveEvent, "move")
public:
    virtual
    void
    update(
            const Path<ScenarioModel>& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) = 0;

    virtual
    const
    Path<ScenarioModel>&
    path() const = 0;
};

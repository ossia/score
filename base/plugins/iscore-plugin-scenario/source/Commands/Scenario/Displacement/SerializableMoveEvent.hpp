#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <ProcessInterface/ExpandMode.hpp>
#include <ProcessInterface/TimeValue.hpp>

class EventModel;
class ScenarioModel;

class SerializableMoveEvent : public iscore::SerializableCommand
{
ISCORE_COMMAND_DECL("ScenarioControl", "move", "move")
public:
    ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SerializableMoveEvent)
    virtual
    void
    update(
            const Path<ScenarioModel>& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) = 0;
};

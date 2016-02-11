#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

namespace Scenario {

class EventModel;
class ScenarioModel;

namespace Command
{
class SerializableMoveEvent : public iscore::SerializableCommand
{
public:
    virtual
    void
    update(
            const Path<Scenario::ScenarioModel>& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            double y,
            ExpandMode mode) = 0;

    virtual
    const
    Path<Scenario::ScenarioModel>&
    path() const = 0;
};
}
}

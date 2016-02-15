#pragma once

#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>
#include "MoveEventFactoryInterface.hpp"

#include <iscore/tools/SettableIdentifier.hpp>
class DataStreamInput;
class DataStreamOutput;

namespace Scenario
{
class EventModel;
class ScenarioModel;
namespace Command
{

class MoveEventMeta final : public SerializableMoveEvent
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveEventMeta, "Move an event")

public:

    MoveEventMeta(
            Path<Scenario::ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            double y,
            ExpandMode mode);

    void undo() const override;
    void redo() const override;

    const Path<Scenario::ScenarioModel>& path() const override;

    // SerializableCommand interface
protected:
    void serializeImpl(DataStreamInput&) const override;
    void deserializeImpl(DataStreamOutput&) override;

private:
    // TODO : make a UI to change that
    Path<Scenario::ScenarioModel> m_scenario;
    Id<EventModel> m_eventId;
    double m_oldY{};
    double m_newY{};

    SerializableMoveEvent* m_moveEventImplementation{};

    // SerializableMoveEvent interface
public:
    void update(const Path<Scenario::ScenarioModel>& scenarioPath, const Id<EventModel>& eventId, const TimeValue& newDate, double y, ExpandMode mode) override;
};
}
}

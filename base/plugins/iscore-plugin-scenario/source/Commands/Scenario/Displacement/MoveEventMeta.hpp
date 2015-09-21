#pragma once

//#include <iscore/command/SerializableCommand.hpp>
#include <Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

class MoveEventMeta : SerializableMoveEvent
{
    // Command interface
public:
    MoveEventMeta(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode);

    void undo();
    void redo();

    // SerializableCommand interface
protected:
    void serializeImpl(QDataStream&) const;
    void deserializeImpl(QDataStream&);

private:
    SerializableMoveEvent* m_moveEventImplementation;

    // SerializableMoveEvent interface
public:
    virtual void update(const Path<ScenarioModel>& scenarioPath, const Id<EventModel>& eventId, const TimeValue& newDate, ExpandMode mode);
};

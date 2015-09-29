#pragma once

#include <Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

class MoveEventOnCreationMeta : public SerializableMoveEvent
{
    // Command interface
public:
    MoveEventOnCreationMeta()
        :SerializableMoveEvent{}
    {}

    MoveEventOnCreationMeta(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode);

    void undo();
    void redo();

    const Path<ScenarioModel>& path() const;

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

#pragma once

#include <Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

class MoveEventMeta : public SerializableMoveEvent
{
    // Command interface
public:
    MoveEventMeta()
        :SerializableMoveEvent{}
    {}

    MoveEventMeta(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode);

    void undo() const override;
    void redo() const override;

    const Path<ScenarioModel>& path() const override;

    // SerializableCommand interface
protected:
    void serializeImpl(QDataStream&) const override;
    void deserializeImpl(QDataStream&) override;

private:
    SerializableMoveEvent* m_moveEventImplementation;

    // SerializableMoveEvent interface
public:
    virtual void update(const Path<ScenarioModel>& scenarioPath, const Id<EventModel>& eventId, const TimeValue& newDate, ExpandMode mode) override;
};

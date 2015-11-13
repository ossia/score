#pragma once

#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

class MoveEventMeta final : public SerializableMoveEvent
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveEventMeta, "MoveEventMeta")

public:

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
    void update(const Path<ScenarioModel>& scenarioPath, const Id<EventModel>& eventId, const TimeValue& newDate, ExpandMode mode) override;
};

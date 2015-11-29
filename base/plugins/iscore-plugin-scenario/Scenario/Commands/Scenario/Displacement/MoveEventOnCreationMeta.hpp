#pragma once

#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include "Process/ExpandMode.hpp"
#include "Process/TimeValue.hpp"
#include "Scenario/Commands/ScenarioCommandFactory.hpp"
#include "iscore/tools/ModelPath.hpp"

class DataStreamInput;
class DataStreamOutput;
class EventModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
template <typename tag, typename impl> class id_base_t;

class MoveEventOnCreationMeta final : public SerializableMoveEvent
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveEventOnCreationMeta, "Move an event on creation")
public:
    MoveEventOnCreationMeta(
            Path<Scenario::ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode);

    void undo() const override;
    void redo() const override;

    const Path<Scenario::ScenarioModel>& path() const override;

    // SerializableCommand interface
protected:
    void serializeImpl(DataStreamInput&) const override;
    void deserializeImpl(DataStreamOutput&) override;

private:
    SerializableMoveEvent* m_moveEventImplementation;

    // SerializableMoveEvent interface
public:
    void update(const Path<Scenario::ScenarioModel>& scenarioPath, const Id<EventModel>& eventId, const TimeValue& newDate, ExpandMode mode) override;
};

#include <MoveEventCSPFactory.hpp>
#include <CSPDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

Scenario::Command::SerializableMoveEvent* MoveEventCSPFactory::make(
        Path<Scenario::ScenarioModel> &&scenarioPath,
        const Id<Scenario::EventModel> &eventId,
        const TimeValue &newDate,
        ExpandMode mode)
{
    return new Scenario::Command::MoveEvent<CSPDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

Scenario::Command::SerializableMoveEvent* MoveEventCSPFactory::make()
{
    return new Scenario::Command::MoveEvent<CSPDisplacementPolicy>();
}

const Scenario::Command::MoveEventFactoryKey& MoveEventCSPFactory::key_impl() const
{
    static const Scenario::Command::MoveEventFactoryKey str{"CSPFlex"};
    return str;
}

#include <MoveEventCSPFlexFactory.hpp>
#include <CSPFlexDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

Scenario::Command::SerializableMoveEvent* MoveEventCSPFlexFactory::make(
        Path<Scenario::ScenarioModel> &&scenarioPath,
        const Id<Scenario::EventModel> &eventId,
        const TimeValue &newDate,
        ExpandMode mode)
{
    return new Scenario::Command::MoveEvent<CSPFlexDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

Scenario::Command::SerializableMoveEvent* MoveEventCSPFlexFactory::make()
{
    return new Scenario::Command::MoveEvent<CSPFlexDisplacementPolicy>();
}

const Scenario::Command::MoveEventFactoryKey& MoveEventCSPFlexFactory::key_impl() const
{
    // TODO why is CSP CSPFlex and CSPFlex CSP
    static const Scenario::Command::MoveEventFactoryKey str{"CSP"};
    return str;
}

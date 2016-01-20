#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename Object> class Path;

namespace Scenario {
class ScenarioModel;
class EventModel;

namespace Command
{
class SerializableMoveEvent;

class MoveEventClassicFactory final : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(
            Path<Scenario::ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) override;

    SerializableMoveEvent* make() override;

    int priority(MoveEventFactoryInterface::Strategy strategy) override
    {
        switch(strategy)
        {
            case MoveEventFactoryInterface::Strategy::CREATION:
                return 0;
                break;
            case MoveEventFactoryInterface::Strategy::MOVING_CLASSIC:
                return 0;
                break;
            default:
                return 0;// not suited for other strategies
                break;
        }
    }

    const MoveEventFactoryKey& key_impl() const override;
};
}
}

#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

class MoveEventClassicFactory final : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) override;

    SerializableMoveEvent* make() override;

    int priority(MoveEventFactoryInterface::Strategy strategy) override
    {
        switch(strategy)
        {
            case MoveEventFactoryInterface::Strategy::CREATION:
            case MoveEventFactoryInterface::Strategy::MOVING:
            case MoveEventFactoryInterface::Strategy::EXTRA:
            default:
                return 0;// not suited for other strategies
                break;
        }
    }

    const std::string& key_impl() const override
    {
        static std::string str{"Classic"};
        return str;
    }
};

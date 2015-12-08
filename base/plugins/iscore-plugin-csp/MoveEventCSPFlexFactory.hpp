#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>


class MoveEventCSPFlexFactory : public MoveEventFactoryInterface
{
        // MoveEventFactory interface
    public:
        SerializableMoveEvent* make(
                Path<Scenario::ScenarioModel> &&scenarioPath,
                const Id<EventModel> &eventId,
                const TimeValue &newDate,
                ExpandMode mode) override;

        SerializableMoveEvent* make() override;

        int priority(MoveEventFactoryInterface::Strategy strategy) override
        {
            switch(strategy)
            {
                case MoveEventFactoryInterface::Strategy::CREATION:
                    return 10;
                    break;
                case MoveEventFactoryInterface::Strategy::MOVING:
                    return 5;
                    break;
                default:
                    return -1;// not suited for other strategies
                    break;
            }
        }


        const MoveEventFactoryKey& key_impl() const override;
};

#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>


class MoveEventCSPFlexFactory : public Scenario::Command::MoveEventFactoryInterface
{
        // MoveEventFactory interface
    public:
        Scenario::Command::SerializableMoveEvent* make(
                Path<Scenario::ScenarioModel> &&scenarioPath,
                const Id<Scenario::EventModel> &eventId,
                const TimeValue &newDate,
                ExpandMode mode) override;

        Scenario::Command::SerializableMoveEvent* make() override;

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


        const Scenario::Command::MoveEventFactoryKey& key_impl() const override;
};

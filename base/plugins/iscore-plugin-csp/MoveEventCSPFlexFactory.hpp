#pragma once

#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Commands/Scenario/Displacement/MoveEventList.hpp>


class MoveEventCSPFlexFactory : public MoveEventFactoryInterface
{
        // MoveEventFactory interface
    public:
        SerializableMoveEvent* make(Path<ScenarioModel> &&scenarioPath, const Id<EventModel> &eventId, const TimeValue &newDate, ExpandMode mode);

        SerializableMoveEvent* make();

        virtual int priority(MoveEventList::Strategy strategy)
        {
            switch(strategy)
            {
                case MoveEventList::Strategy::CREATION:
                    return 10;
                    break;
                case MoveEventList::Strategy::MOVING:
                    return 5;
                    break;
                default:
                    return -1;// not suited for other strategies
                    break;
            }
        }

        QString name() const override
        { return "CSP"; }
};

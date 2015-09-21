#pragma once

#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>


class MoveEventCSPFactory : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    iscore::SerializableCommand* make(Path<ScenarioModel> &&scenarioPath, const Id<EventModel> &eventId, const TimeValue &newDate, ExpandMode mode);

    virtual int priority()
    {
         return 10;
    }

    QString name() const override
    { return "CSP"; }
};

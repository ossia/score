#pragma once

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
//#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ExpandMode.hpp>

class ScenarioModel;
class EventModel;


class MoveEventFactoryInterface : public iscore::GenericFactoryInterface<std::string>
{
        ISCORE_FACTORY_DECL("MoveEvent")
public:
            enum Strategy{ MOVING, CREATION, EXTRA };

    virtual SerializableMoveEvent* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) = 0;

    virtual SerializableMoveEvent* make() = 0;

    virtual ~MoveEventFactoryInterface();

     /**
     * @brief priority
     * the highest priority will be default move behavior for the indicated strategy
     * Basically, we want to know how well the policy is suited for the desired strategy.
     * @param strategy
     * the strategy for which we need a displacement policy;
     * @return
     */
    virtual int priority(Strategy strategy) = 0;

};

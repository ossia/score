#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

template <typename Object> class Path;
namespace Scenario
{
class ScenarioModel;
class EventModel;
class ScenarioModel;
namespace Command
{
class MoveEventTag{};
using MoveEventFactoryKey = StringKey<MoveEventTag>;

class SerializableMoveEvent;


class ISCORE_PLUGIN_SCENARIO_EXPORT MoveEventFactoryInterface : public iscore::GenericFactoryInterface<MoveEventFactoryKey>
{
        ISCORE_FACTORY_DECL("MoveEvent")
public:
            enum Strategy{ MOVING, CREATION, EXTRA };

    virtual SerializableMoveEvent* make(
            Path<Scenario::ScenarioModel>&& scenarioPath,
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
}
}

Q_DECLARE_METATYPE(Scenario::Command::MoveEventFactoryKey)

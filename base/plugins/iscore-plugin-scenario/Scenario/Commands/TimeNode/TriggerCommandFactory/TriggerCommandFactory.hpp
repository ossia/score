#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_scenario_export.h>
namespace iscore
{
class SerializableCommand;
}
namespace Scenario
{
class TimeNodeModel;
namespace Command
{

class ISCORE_PLUGIN_SCENARIO_EXPORT TriggerCommandFactory : public iscore::FactoryInterfaceBase
{
        ISCORE_FACTORY_DECL("TriggerCommandFactory")
    public:
        virtual ~TriggerCommandFactory();
        virtual bool matches(
                const TimeNodeModel& tn) const = 0;
        virtual iscore::SerializableCommand* make_addTriggerCommand(
                const TimeNodeModel& tn) const = 0;
        virtual iscore::SerializableCommand* make_removeTriggerCommand(
                const TimeNodeModel& tn) const = 0;
};

}
}

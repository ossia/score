#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class TimeNodeModel;
namespace iscore
{
class SerializableCommand;
}
class TriggerCommandFactory : public iscore::FactoryInterfaceBase
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


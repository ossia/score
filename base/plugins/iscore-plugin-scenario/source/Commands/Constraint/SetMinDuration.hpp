#pragma once
#include <iscore/command/PropertyCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>
namespace Scenario
{
    namespace Command
    {
    /**
     * @brief The SetMinDuration class
     *
     * Sets the Min duration of a Constraint
     */
    class SetMinDuration : public iscore::PropertyCommand
    {
            ISCORE_COMMAND_DECL("SetMinDuration", "Set constraint minimum")
        public:
            ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetMinDuration, "ScenarioControl")

            SetMinDuration(ObjectPath&& path, const TimeValue& newval):
                iscore::PropertyCommand{
                std::move(path), "minDuration", QVariant::fromValue(newval), "ScenarioControl", commandName(), description()}
            {

            }

            void update(const ObjectPath & p, const TimeValue &newval)
            {
                iscore::PropertyCommand::update(p, QVariant::fromValue(newval));
            }
    };
    }
}

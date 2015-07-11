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
     * @brief The SetMaxDuration class
     *
     * Sets the Max duration of a Constraint
     */
    class SetMaxDuration : public iscore::PropertyCommand
    {
            ISCORE_COMMAND_DECL("SetMaxDuration", "Set constraint maximum")
        public:
            ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetMaxDuration, "ScenarioControl")

            SetMaxDuration(ObjectPath&& path, const TimeValue& newval):
                iscore::PropertyCommand{
                std::move(path), "maxDuration", QVariant::fromValue(newval), "ScenarioControl", commandName(), description()}
            {

            }

            void update(const ObjectPath & p, const TimeValue &newval)
            {
                iscore::PropertyCommand::update(p, QVariant::fromValue(newval));
            }
    };
    }
}

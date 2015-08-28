#pragma once
#include <iscore/command/PropertyCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ConstraintModel;
namespace Scenario
{
    namespace Command
    {

    /**
     * @brief The SetMaxDuration class
     *
     * Sets the Max duration of a Constraint
     */

    // TODO this does not work anymore since the properties have been moved in a sub-object
    class SetMaxDuration : public iscore::PropertyCommand
    {
            ISCORE_COMMAND_DECL("SetMaxDuration", "Set constraint maximum")
        public:
            ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetMaxDuration, "ScenarioControl")

            SetMaxDuration(Path<ConstraintModel>&& path, const TimeValue& newval):
                iscore::PropertyCommand{
                std::move(path), "maxDuration", QVariant::fromValue(newval), "ScenarioControl", commandName(), description()}
            {

            }

            void update(const Path<ConstraintModel>& p, const TimeValue &newval)
            {
                iscore::PropertyCommand::update(p, QVariant::fromValue(newval));
            }
    };
    }
}

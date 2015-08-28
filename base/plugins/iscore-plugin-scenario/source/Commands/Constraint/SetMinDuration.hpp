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
     * @brief The SetMinDuration class
     *
     * Sets the Min duration of a Constraint
     */
    // TODO this does not work anymore since the properties have been moved in a sub-object
    class SetMinDuration : public iscore::PropertyCommand
    {
            ISCORE_COMMAND_DECL_OBSOLETE("SetMinDuration", "Set constraint minimum")
        public:
            ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetMinDuration, "ScenarioControl")

            SetMinDuration(Path<ConstraintModel>&& path, const TimeValue& newval):
                iscore::PropertyCommand{
                std::move(path), "minDuration", QVariant::fromValue(newval), "ScenarioControl", commandName(), description()}
            {

            }

            void update(const Path<ConstraintModel> & p, const TimeValue &newval)
            {
                iscore::PropertyCommand::update(p, QVariant::fromValue(newval));
            }
    };
    }
}

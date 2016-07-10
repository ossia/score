#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
/**
 * @brief The AutomationDropHandler class
 * Will create an automation where the addresses are dropped.
 */
class AutomationDropHandler final :
        public ConstraintDropHandler
{
        ISCORE_CONCRETE_FACTORY("851c98e1-4bcb-407b-9a72-8288d83c9f38")

        bool handle(
                const Scenario::ConstraintModel&,
                const QMimeData* mime) override;
};
}

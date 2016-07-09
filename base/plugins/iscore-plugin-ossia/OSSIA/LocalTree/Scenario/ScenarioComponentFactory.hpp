#pragma once
#include <OSSIA/LocalTree/Scenario/ScenarioComponent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
namespace Ossia
{
namespace LocalTree
{
LOCALTREE_PROCESS_COMPONENT_FACTORY(
        ScenarioComponentFactory,
        "ffc0aaed-9197-4956-a966-3f54afdfb762",
        ScenarioComponent,
        Scenario::ProcessModel)
}
}

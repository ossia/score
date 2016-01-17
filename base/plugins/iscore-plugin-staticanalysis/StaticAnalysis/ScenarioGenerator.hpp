#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace Scenario
{
class ScenarioModel;
}
namespace stal
{
    void generateScenario(
            const Scenario::ScenarioModel& scenar,
            int N,
            CommandDispatcher<>&);
}

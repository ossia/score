#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace Scenario
{
    class ScenarioModel;

    void generateScenario(const ScenarioModel& scenar, int N, CommandDispatcher<>&);
}

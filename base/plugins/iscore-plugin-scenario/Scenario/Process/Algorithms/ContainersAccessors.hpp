#pragma once
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

namespace Scenario
{

inline static auto& getConstraints(
        const ScenarioModel& target)
{
    return target.constraints;
}

inline static auto& getStates(
        const ScenarioModel& target)
{
    return target.states;
}

inline static auto& getEvents(
        const ScenarioModel& target)
{
    return target.events;
}

inline static auto& getTimeNodes(
        const ScenarioModel& target)
{
    return target.timeNodes;
}

inline static auto getConstraints(
        const BaseScenarioContainer& target)
{
    return target.constraints();
}

inline static auto getStates(
        const BaseScenarioContainer& target)
{
    return target.states();
}

inline static auto getEvents(
        const BaseScenarioContainer& target)
{
    return target.events();
}

inline static auto getTimeNodes(
        const BaseScenarioContainer& target)
{
    return target.timeNodes();
}

}

#pragma once
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{

inline static auto& getIntervals(const ProcessModel& target)
{
  return target.intervals;
}

inline static auto& getStates(const ProcessModel& target)
{
  return target.states;
}

inline static auto& getEvents(const ProcessModel& target)
{
  return target.events;
}

inline static auto& getTimeSyncs(const ProcessModel& target)
{
  return target.timeSyncs;
}

inline static auto getIntervals(const BaseScenarioContainer& target)
{
  return target.intervals();
}

inline static auto getStates(const BaseScenarioContainer& target)
{
  return target.states();
}

inline static auto getEvents(const BaseScenarioContainer& target)
{
  return target.events();
}

inline static auto getTimeSyncs(const BaseScenarioContainer& target)
{
  return target.timeSyncs();
}

/**
 * \class ScenarioRecursiveFind
 *
 * Will find recursively all the elements of a given type in an score
 * process hierarchy.
 *
 */
template <typename T>
class ScenarioRecursiveFind
{
public:
  std::vector<T*> elements;

  void visit(Scenario::ScenarioInterface& s)
  {
    using type = Scenario::ElementTraits<Scenario::ScenarioInterface, T>;
    const auto& sc = (s.*type::accessor)();
    elements.reserve(elements.size() + sc.size());
    for (auto& e : sc)
    {
      elements.push_back(&e);
      visit(e);
    }
  }

  void visit(Scenario::IntervalModel& c)
  {
    for (auto& proc : c.processes)
    {
      if (auto scenario = dynamic_cast<Scenario::ScenarioInterface*>(&proc))
      {
        visit(*scenario);
      }
    }
  }
};
}

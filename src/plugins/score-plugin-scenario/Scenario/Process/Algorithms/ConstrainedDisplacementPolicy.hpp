#pragma once
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Tools/dataStructures.hpp>

namespace Scenario
{

class ConstrainedDisplacementPolicy
{
public:
  static void
  init(Scenario::ProcessModel& scenario, const QVector<Id<TimeSyncModel>>& draggedElements);

  static void computeDisplacement(
      Scenario::ProcessModel& scenario,
      const QVector<Id<TimeSyncModel>>& draggedElements,
      const TimeVal& deltaTime,
      ElementsProperties& elementsProperties);

  static QString name();

  template <typename... Args>
  static void updatePositions(Args&&... args)
  {
    CommonDisplacementPolicy::updatePositions(std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void revertPositions(Args&&... args)
  {
    CommonDisplacementPolicy::revertPositions(std::forward<Args>(args)...);
  }
};
}

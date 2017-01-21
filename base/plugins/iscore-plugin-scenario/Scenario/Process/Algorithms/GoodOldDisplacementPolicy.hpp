#pragma once

#include <QString>
#include <QVector>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <algorithm>
#include <vector>

#include <Process/TimeValue.hpp>

#include <iscore/model/Identifier.hpp>

namespace Scenario
{
struct ElementsProperties;
class TimeNodeModel;
class ProcessModel;
class GoodOldDisplacementPolicy
{
public:
  static void init(
      Scenario::ProcessModel& scenario,
      const QVector<Id<TimeNodeModel>>& draggedElements)
  {
  }

  static void computeDisplacement(
      Scenario::ProcessModel& scenario,
      const QVector<Id<TimeNodeModel>>& draggedElements,
      const TimeVal& deltaTime,
      ElementsProperties& elementsProperties);

  static void getRelatedTimeNodes(
      Scenario::ProcessModel& scenario,
      const Id<TimeNodeModel>& firstTimeNodeMovedId,
      std::vector<Id<TimeNodeModel>>& translatedTimeNodes);

  static QString name()
  {
    return QString{"Old way"};
  }

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

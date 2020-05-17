#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <score/model/Identifier.hpp>

#include <QString>
#include <QVector>

#include <vector>

namespace Scenario
{
struct ElementsProperties;
class TimeSyncModel;
class ProcessModel;
class GoodOldDisplacementPolicy
{
public:
  static void
  init(Scenario::ProcessModel& scenario, const QVector<Id<TimeSyncModel>>& draggedElements)
  {
  }

  static void computeDisplacement(
      Scenario::ProcessModel& scenario,
      const QVector<Id<TimeSyncModel>>& draggedElements,
      const TimeVal& deltaTime,
      ElementsProperties& elementsProperties);

  static void getRelatedTimeSyncs(
      Scenario::ProcessModel& scenario,
      const Id<TimeSyncModel>& firstTimeSyncMovedId,
      std::vector<Id<TimeSyncModel>>& translatedTimeSyncs);

  static QString name() { return QString{"Old way"}; }

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

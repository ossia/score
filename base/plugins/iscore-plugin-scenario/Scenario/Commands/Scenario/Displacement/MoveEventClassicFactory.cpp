// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>

#include <QString>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>

namespace Scenario
{
class ProcessModel;
class EventModel;
namespace Command
{
class SerializableMoveEvent;

class MinimalDisplacementPolicy
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
      ElementsProperties& elementsProperties)
  {
      // Scale all the constraints before and after.
      if(draggedElements.empty())
          return;
      auto tn_id = draggedElements[0];
      auto& tn = scenario.timeNodes.at(tn_id);
      const auto& constraintsBefore = Scenario::previousConstraints(tn, scenario);
      const auto& constraintsAfter = Scenario::nextConstraints(tn, scenario);

      // 1. Find the delta bounds.
      // We have to stop as soon as a constraint would become too small.
      TimeVal min = TimeVal::infinite();
      TimeVal max = TimeVal::infinite();
      for(auto& id : constraintsBefore)
      {
          auto& c = scenario.constraints.at(id);
          if(c.duration.defaultDuration() < min)
              min = c.duration.defaultDuration();
      }

      for(auto& id : constraintsAfter)
      {
          auto& c = scenario.constraints.at(id);
          if(c.duration.defaultDuration() < max)
              max = c.duration.defaultDuration();
      }

      // 2. Rescale deltaTime
      auto dt = deltaTime;
      if(min != TimeVal::infinite() && dt < TimeVal::zero() && dt < -min)
      {
          dt = -min;
      }
      else if(max != TimeVal::infinite() && dt > TimeVal::zero() && dt > max)
      {
          dt = max;
      }


      for(auto& id : constraintsBefore)
      {
          auto it = elementsProperties.constraints.find(id);
          if(it != elementsProperties.constraints.end())
          {
              auto& c = it.value();
              c.newMin = std::max(TimeVal::zero(), c.oldMin + dt);
              c.newMax = c.oldMax + dt;
          }
          else
          {
              auto& curConstraint = scenario.constraints.at(id);
              ConstraintProperties c{curConstraint};
              c.oldMin = curConstraint.duration.minDuration();
              c.oldMax = curConstraint.duration.maxDuration();
              c.newMin = std::max(TimeVal::zero(), c.oldMin + dt);
              c.newMax = c.oldMax + dt;
              elementsProperties.constraints.insert({id, c});
          }
      }

      for(auto& id : constraintsAfter)
      {
          auto it = elementsProperties.constraints.find(id);
          if(it != elementsProperties.constraints.end())
          {
              auto& c = it.value();
              c.newMin = std::max(TimeVal::zero(), c.oldMin + dt);
              c.newMax = c.oldMax + dt;
          }
          else
          {
              auto& curConstraint = scenario.constraints.at(id);
              ConstraintProperties c{curConstraint};
              c.oldMin = curConstraint.duration.minDuration();
              c.oldMax = curConstraint.duration.maxDuration();
              c.newMin = std::max(TimeVal::zero(), c.oldMin + dt);
              c.newMax = c.oldMax + dt;
              elementsProperties.constraints.insert({id, c});
          }
      }

      auto it = elementsProperties.timenodes.find(tn.id());
      if(it != elementsProperties.timenodes.end())
      {
          it.value().newDate = it.value().oldDate + dt;
      }
      else
      {
          TimenodeProperties t;

          t.oldDate = tn.date();
          t.newDate = t.oldDate + dt;
          elementsProperties.timenodes.insert({tn.id(), t});
      }
  }

  static QString name()
  {
    return QString{"Minimal way"};
  }

  template <typename ProcessScaleMethod>
  static void updatePositions(
          Scenario::ProcessModel& scenario,
          ProcessScaleMethod&& scaleMethod,
          const ElementsProperties& propsToUpdate)
  {
      // update each affected timenodes
      for (auto it = propsToUpdate.timenodes.cbegin();
           it != propsToUpdate.timenodes.cend();
           ++it)
      {
        auto& curTn = scenario.timeNode(it.key());
        const TimenodeProperties& curTn_prop = it.value();

        curTn.setDate(curTn_prop.newDate);

        // update related events
        for (const auto& event : curTn.events())
        {
          scenario.events.at(event).setDate(curTn_prop.newDate);
        }
      }

      // update affected constraints
      for(auto& e : propsToUpdate.constraints)
      {
        auto curCst_id = e.first;

        auto& curCst = scenario.constraints.at(curCst_id);
        const ConstraintProperties& curCst_prop = e.second;

        // compute default duration here
        const auto& startDate
            = scenario
                  .event(scenario.state(curCst.startState())
                             .eventId())
                  .date();
        const auto& endDate
            = scenario
                  .event(
                      scenario.state(curCst.endState()).eventId())
                  .date();

        TimeVal defaultDuration = endDate - startDate;

        // set start date and default duration
        if (!(curCst.startDate() - startDate).isZero())
        {
          curCst.setStartDate(startDate);
        }
        curCst.duration.setDefaultDuration(defaultDuration);
        curCst.duration.setMinDuration(curCst_prop.newMin);
        curCst.duration.setMaxDuration(curCst_prop.newMax);

        for (auto& process : curCst.processes)
        {
          scaleMethod(process, defaultDuration);
        }

        emit scenario.constraintMoved(curCst);
      }
  }

  template <typename ProcessScaleMethod>
  static void revertPositions(
          const iscore::DocumentContext& ctx,
          Scenario::ProcessModel& scenario,
          ProcessScaleMethod&& scaleMethod,
          const ElementsProperties& propsToUpdate)
  {
      // update each affected timenodes with old values
      for (auto it = propsToUpdate.timenodes.cbegin();
           it != propsToUpdate.timenodes.cend();
           ++it)
      {
        auto& curTn = scenario.timeNode(it.key());
        auto& curTn_prop = it.value();

        curTn.setDate(curTn_prop.oldDate);

        // update related events to mach the date
        for (const auto& event : curTn.events())
        {
          scenario.events.at(event).setDate(curTn.date());
        }
      }

      // update affected constraints with old values and restor processes
      for(auto& e : propsToUpdate.constraints)
      {
        auto curConstraintPropertiesToUpdate_id = e.first;

        auto& curConstraintToUpdate
            = scenario.constraints.at(curConstraintPropertiesToUpdate_id);
        auto& curConstraintPropertiesToUpdate = e.second;

        // compute default duration here
        const auto& startDate
            = scenario
                  .event(scenario.state(curConstraintToUpdate.startState())
                             .eventId())
                  .date();
        const auto& endDate
            = scenario
                  .event(
                      scenario.state(curConstraintToUpdate.endState()).eventId())
                  .date();

        TimeVal defaultDuration = endDate - startDate;

        // set start date and default duration
        if (!(curConstraintToUpdate.startDate() - startDate).isZero())
        {
          curConstraintToUpdate.setStartDate(startDate);
        }
        curConstraintToUpdate.duration.setDefaultDuration(defaultDuration);

        // set durations
        curConstraintToUpdate.duration.setMinDuration(
            curConstraintPropertiesToUpdate.oldMin);
        curConstraintToUpdate.duration.setMaxDuration(
            curConstraintPropertiesToUpdate.oldMax);

        // Now we have to restore the state of each constraint that might have
        // been modified
        // during this command.

        // 1. Clear the constraint
        // TODO Don't use a command since it serializes a ton of unused stuff.
        Command::ClearConstraint clear_cmd{curConstraintToUpdate};
        clear_cmd.redo(ctx);

        // 2. Restore the rackes & processes.
        // Restore the constraint. The saving is done in
        // GenericDisplacementPolicy.
        curConstraintPropertiesToUpdate.reload(curConstraintToUpdate);

        emit scenario.constraintMoved(curConstraintToUpdate);
      }
  }
};

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel>
        eventId,
    TimeVal newDate,
    ExpandMode mode)
{
  return std::make_unique<MoveEvent<MinimalDisplacementPolicy>>(
      std::move(scenarioPath), std::move(eventId), std::move(newDate), mode);
}

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make()
{
  return std::make_unique<MoveEvent<MinimalDisplacementPolicy>>();
}
}
}

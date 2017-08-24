#include "ConstrainedDisplacementPolicy.hpp"


namespace Scenario
{

void ConstrainedDisplacementPolicy::init(
        ProcessModel &scenario,
        const QVector<Id<TimeSyncModel> > &draggedElements)
{
}

void ConstrainedDisplacementPolicy::computeDisplacement(
        ProcessModel &scenario,
        const QVector<Id<TimeSyncModel> > &draggedElements,
        const TimeVal &deltaTime,
        ElementsProperties &elementsProperties)
{
    // Scale all the constraints before and after.
    if(draggedElements.empty())
        return;
    auto tn_id = draggedElements[0];
    auto& tn = scenario.timeSyncs.at(tn_id);
    const auto& constraintsBefore = Scenario::previousConstraints(tn, scenario);
    const auto& constraintsAfter = Scenario::nextConstraints(tn, scenario);

    // 1. Find the delta bounds.
    // We have to stop as soon as a constraint would become too small.
    TimeVal min = TimeVal::infinite();
    TimeVal max = TimeVal::infinite();
    for(const auto& id : constraintsBefore)
    {
        auto it = elementsProperties.constraints.find(id);
        if(it == elementsProperties.constraints.end())
        {
            auto& c = scenario.constraints.at(id);
            if(c.duration.defaultDuration() < min)
                min = c.duration.defaultDuration();
        }
        else
        {
            const ConstraintProperties& c = it.value();
            if(c.oldDefault < min)
                min = c.oldDefault;
        }
    }

    for(const auto& id : constraintsAfter)
    {
        auto it = elementsProperties.constraints.find(id);
        if(it == elementsProperties.constraints.end())
        {
            auto& c = scenario.constraints.at(id);
            if(c.duration.defaultDuration() < max)
                max = c.duration.defaultDuration();
        }
        else
        {
            const ConstraintProperties& c = it.value();
            if(c.oldDefault < max)
                max = c.oldDefault;
        }
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
            c.oldDefault = curConstraint.duration.defaultDuration();
            c.oldMin = curConstraint.duration.minDuration();
            c.oldMax = curConstraint.duration.maxDuration();

            c.newMin = c.oldMin;
            c.newMax = c.oldMax;
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
            c.newMin = std::max(TimeVal::zero(), c.oldMin - dt);
            c.newMax = c.oldMax - dt;
        }
        else
        {
            auto& curConstraint = scenario.constraints.at(id);
            ConstraintProperties c{curConstraint};
            c.oldDefault = curConstraint.duration.defaultDuration();
            c.oldMin = curConstraint.duration.minDuration();
            c.oldMax = curConstraint.duration.maxDuration();

            c.newMin = c.oldMin;
            c.newMax = c.oldMax;

            c.newMin = std::max(TimeVal::zero(), c.oldMin - dt);
            c.newMax = c.oldMax - dt;
            elementsProperties.constraints.insert({id, c});
        }
    }

    auto it = elementsProperties.timesyncs.find(tn.id());
    if(it != elementsProperties.timesyncs.end())
    {
        it.value().newDate = it.value().oldDate + dt;
    }
    else
    {
        TimenodeProperties t;

        t.oldDate = tn.date();
        t.newDate = t.oldDate + dt;
        elementsProperties.timesyncs.insert({tn.id(), t});
    }
}

QString ConstrainedDisplacementPolicy::name()
{
    return QString{"Minimal way"};
}



}

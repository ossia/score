#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Process/ScenarioInterface.hpp>
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <kiwi/kiwi.h>

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const Id<ConstraintModel>& constraintId)
{
    m_min.setValue(cspScenario.getScenario()->constraint(constraintId).duration.minDuration().msec());
    m_max.setValue(cspScenario.getScenario()->constraint(constraintId).duration.maxDuration().msec());

    auto scenario = cspScenario.getScenario();
    auto& constraint = scenario->constraint(constraintId);

    auto& prevTimenodeModel = scenario->timeNode(constraint.startTimeNode());
    auto& nextTimenodeModel = scenario->timeNode(constraint.endTimeNode());

    auto prevCSPTimenode = cspScenario.insertTimenode(prevTimenodeModel.id());
    auto nextCSPTimenode = cspScenario.insertTimenode(nextTimenodeModel.id());

    // apply model constraints
    //Note: min & max can be negative no problemo muchacho
    // 1 - min inferior to max
    // 2 - date of end timenode inside min and max
    cspScenario.getSolver().addConstraint(m_min <= m_max);

    cspScenario.getSolver().addConstraint(nextCSPTimenode->getDate() >= (prevCSPTimenode->getDate() + m_min));
    cspScenario.getSolver().addConstraint(nextCSPTimenode->getDate() <= (prevCSPTimenode->getDate() + m_max));

    // if there are sub scenarios, store them
    for(auto& process : constraint.processes)
    {
        if(auto scenario = dynamic_cast<ScenarioModel*>(&process))
        {
            m_subScenarios.push_back(new CSPScenario(*scenario));
        }
    }

    // watch over durations edits
    con(constraint.duration, &ConstraintDurations::minDurationChanged, this, &CSPTimeRelation::onMinDurationChanged);
    con(constraint.duration, &ConstraintDurations::maxDurationChanged, this, &CSPTimeRelation::onMaxDurationChanged);

}

const kiwi::Variable& CSPTimeRelation::getMin() const
{
    return m_min;
}

const kiwi::Variable& CSPTimeRelation::getMax() const
{
    return m_max;
}

void CSPTimeRelation::onMinDurationChanged(const TimeValue& min)
{
}

void CSPTimeRelation::onMaxDurationChanged(const TimeValue& max)
{
}

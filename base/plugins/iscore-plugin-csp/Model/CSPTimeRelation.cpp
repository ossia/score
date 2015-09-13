#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Process/ScenarioInterface.hpp>

#include <Document/Constraint/ConstraintModel.hpp>

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const ConstraintModel& constraint)
{
    auto* startTimenode = cspScenario.getStartTimeNode();
    auto* endTimenode = cspScenario.getEndTimeNode();

    // copy attributes values
    m_min.change_value(constraint.duration.minDuration().msec());
    m_max.change_value(constraint.duration.maxDuration().msec());

    // apply model constraints
    //Note: min & max can be negative no problemo muchacho
    // 1 - min inferior to max
    // 2 - date of end timenode inside min and max
    cspScenario.getSolver().add_constraints(
    {
                    m_min <= m_max,
                    endTimenode->getDate() >= startTimenode->getDate() + m_min,
                    endTimenode->getDate() >= startTimenode->getDate() + m_max

                });

}

rhea::variable CSPTimeRelation::getMin() const
{
    return m_min;
}

rhea::variable CSPTimeRelation::getMax() const
{
    return m_max;
}

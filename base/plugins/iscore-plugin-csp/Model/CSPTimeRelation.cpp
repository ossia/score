#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Process/ScenarioInterface.hpp>

#include <Document/Constraint/ConstraintModel.hpp>

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const ConstraintModel& constraint)
{


    // copy min and max
    m_min.change_value(constraint.duration.minDuration().msec());
    m_max.change_value(constraint.duration.maxDuration().msec());

    //apply constraints relative to the model
    //Note: min & max can be negative no problemo muchacho
    cspScenario.getSolver().add_constraints(
    {
                    m_min <= m_max
                });

}

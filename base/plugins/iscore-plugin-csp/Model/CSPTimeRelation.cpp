#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Process/ScenarioInterface.hpp>
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const Id<ConstraintModel>& constraintId)
    :m_min(cspScenario.getScenario()->constraint(constraintId).duration.minDuration().msec()),
      m_max(cspScenario.getScenario()->constraint(constraintId).duration.maxDuration().msec())
{
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
    auto cstrnts = rhea::constraint_list{
        m_min <= m_max,
        nextCSPTimenode->getDate() >= (prevCSPTimenode->getDate() + m_min),
        nextCSPTimenode->getDate() <= (prevCSPTimenode->getDate() + m_max)

    };
    cspScenario.getSolver().add_constraints(cstrnts
    );

    // if there are sub scenarios, store them
    for(auto& process : constraint.processes)
    {
        if(auto scenario = dynamic_cast<ScenarioModel*>(&process))
        {
            m_subScenarios.push_back(new CSPScenario(*scenario));
        }
    }
}

const rhea::variable& CSPTimeRelation::getMin() const
{
    return m_min;
}

const rhea::variable& CSPTimeRelation::getMax() const
{
    return m_max;
}

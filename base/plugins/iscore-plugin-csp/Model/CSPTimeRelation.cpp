#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Process/ScenarioInterface.hpp>
#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Model/CSPTimeNode.hpp>
#include <kiwi/kiwi.h>

#define PUT_CONSTRAINT(constraintName, constraint) \
    kiwi::Constraint* constraintName = new kiwi::Constraint(constraint);\
    m_solver.addConstraint(*constraintName);\
    m_constraints.push_back(constraintName)

#define STAY_STRENGTH kiwi::strength::medium

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const Id<ConstraintModel>& constraintId)
    :CSPConstraintHolder::CSPConstraintHolder(&cspScenario),
      m_solver(cspScenario.getSolver())
{
    this->setParent(&cspScenario);
    this->setObjectName("CSPTimeRelation");

    m_iscoreMin = &cspScenario.getScenario()->constraint(constraintId).duration.minDuration();
    m_iscoreMax = &cspScenario.getScenario()->constraint(constraintId).duration.maxDuration();

    m_min.setValue(m_iscoreMin->msec());
    m_max.setValue(m_iscoreMax->msec());

    // weight
    //solver.addEditVariable(m_min, kiwi::strength::strong);
    //solver.addEditVariable(m_max, kiwi::strength::medium);

    auto scenario = cspScenario.getScenario();
    auto& constraint = scenario->constraint(constraintId);

    auto& prevTimenodeModel = scenario->timeNode(constraint.startTimeNode());
    auto& nextTimenodeModel = scenario->timeNode(constraint.endTimeNode());

    auto prevCSPTimenode = cspScenario.insertTimenode(prevTimenodeModel.id());
    auto nextCSPTimenode = cspScenario.insertTimenode(nextTimenodeModel.id());

    // apply model constraints
    //Note: min & max can be negative no problemo muchacho
    // 1 - min inferior to max
    PUT_CONSTRAINT(cMinInfMax, m_min <= m_max);

    // 2 - date of end timenode inside min and max
    PUT_CONSTRAINT(cInsideMin, nextCSPTimenode->getDate() >= (prevCSPTimenode->getDate() + m_min));
    PUT_CONSTRAINT(cInsideMax, nextCSPTimenode->getDate() <= (prevCSPTimenode->getDate() + m_max));

    // 3 - min >= 0
    PUT_CONSTRAINT(cMinSupZero, m_min >= 0);

    // 4 - STAY bahavior
    m_cstrStayMin = new kiwi::Constraint(m_min == m_min.value(), STAY_STRENGTH); // try keeping interval
    m_solver.addConstraint(*m_cstrStayMin);
    m_cstrStayMax = new kiwi::Constraint(m_max == m_max.value(), STAY_STRENGTH);
    m_solver.addConstraint(*m_cstrStayMax);



    // if there are sub scenarios, store them
    for(auto& process : constraint.processes)
    {
        if(auto scenario = dynamic_cast<ScenarioModel*>(&process))
        {
            m_subScenarios.push_back(new CSPScenario(*scenario, scenario));
        }
    }

    // watch over durations edits
    con(constraint.duration, &ConstraintDurations::minDurationChanged, this, &CSPTimeRelation::onMinDurationChanged);
    con(constraint.duration, &ConstraintDurations::maxDurationChanged, this, &CSPTimeRelation::onMaxDurationChanged);
}

CSPTimeRelation::~CSPTimeRelation()
{
    delete(m_cstrStayMin);
    delete(m_cstrStayMax);
}

kiwi::Variable& CSPTimeRelation::getMin()
{
    return m_min;
}

kiwi::Variable& CSPTimeRelation::getMax()
{
    return m_max;
}

bool CSPTimeRelation::minChanged() const
{
    return m_min.value() != m_iscoreMin->msec();
}

bool CSPTimeRelation::maxChanged() const
{
    return m_max.value() != m_iscoreMax->msec();
}

void CSPTimeRelation::onMinDurationChanged(const TimeValue& min)
{
    m_min.setValue(min.msec());

    // refresh min stay contraint
    m_solver.removeConstraint(*m_cstrStayMin);

    m_cstrStayMin = new kiwi::Constraint(m_min == m_min.value(), STAY_STRENGTH);
    m_solver.addConstraint(*m_cstrStayMin);

}

void CSPTimeRelation::onMaxDurationChanged(const TimeValue& max)
{
    if(max.isInfinite())
    {
        //TODO : ??? remove constraints on max?
    }else
    {
        m_max.setValue(max.msec());

        // refresh max stay contraint
        m_solver.removeConstraint(*m_cstrStayMax);

        m_cstrStayMax = new kiwi::Constraint(m_max == m_max.value(), STAY_STRENGTH);
        m_solver.addConstraint(*m_cstrStayMax);
    }
}

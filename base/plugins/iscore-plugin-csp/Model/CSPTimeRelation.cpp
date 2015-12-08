#include "CSPTimeRelation.hpp"
#include "CSPScenario.hpp"
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Model/CSPTimeNode.hpp>
#include <kiwi/kiwi.h>
#include <Scenario/Process/Algorithms/Accessors.hpp>

CSPTimeRelation::CSPTimeRelation(CSPScenario& cspScenario, const Id<ConstraintModel>& constraintId)
    :CSPConstraintHolder::CSPConstraintHolder(cspScenario.getSolver(), &cspScenario)
{
    qDebug("coucou");
    this->setParent(&cspScenario);
    this->setObjectName("CSPTimeRelation");

    m_iscoreMin = cspScenario.getScenario()->constraint(constraintId).duration.minDuration();
    m_iscoreMax = cspScenario.getScenario()->constraint(constraintId).duration.maxDuration();

    m_min.setValue(m_iscoreMin.msec());
    m_max.setValue(m_iscoreMax.msec());

    // weight
    //solver.addEditVariable(m_min, kiwi::strength::strong);
    //solver.addEditVariable(m_max, kiwi::strength::medium);

    auto& scenario = *cspScenario.getScenario();
    auto& constraint = scenario.constraint(constraintId);

    auto& prevTimenodeModel = startTimeNode(constraint, scenario);
    auto& nextTimenodeModel = endTimeNode(constraint, scenario);

    //retrieve/create prev and next timenode
    auto prevCSPTimenode = cspScenario.insertTimenode(prevTimenodeModel.id());
    auto nextCSPTimenode = cspScenario.insertTimenode(nextTimenodeModel.id());

    // apply model constraints
    // 1 - min >= 0
    PUT_CONSTRAINT(cMinSupZero, m_min >= 0);

    // 2 - min inferior to max
    PUT_CONSTRAINT(cMinInfMax, m_min <= m_max);

    // 3 - date of end timenode inside min and max
    PUT_CONSTRAINT(cNextDateMin, nextCSPTimenode->getDate() >= (prevCSPTimenode->getDate() + m_min));
    PUT_CONSTRAINT(cNextDateMax, nextCSPTimenode->getDate() <= (prevCSPTimenode->getDate() + m_max));


    // if there are sub scenarios, store them
    for(auto& process : constraint.processes)
    {
        if(auto* scenar = dynamic_cast<Scenario::ScenarioModel*>(&process))
        {
            m_subScenarios.insert(scenar->id(), new CSPScenario(*scenar, scenar));
        }
    }

    // watch over durations edits
    con(constraint.duration, &ConstraintDurations::minDurationChanged, this, &CSPTimeRelation::onMinDurationChanged);
    con(constraint.duration, &ConstraintDurations::maxDurationChanged, this, &CSPTimeRelation::onMaxDurationChanged);

    // watch over potential subscenarios
    constraint.processes.added.connect<CSPTimeRelation, &CSPTimeRelation::onProcessCreated>(this);
    constraint.processes.removed.connect<CSPTimeRelation, &CSPTimeRelation::onProcessRemoved>(this);
}

CSPTimeRelation::~CSPTimeRelation()
{
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
    return m_min.value() != m_iscoreMin.msec();
}

bool CSPTimeRelation::maxChanged() const
{
    return m_max.value() != m_iscoreMax.msec();
}

void CSPTimeRelation::onMinDurationChanged(const TimeValue& min)
{
    m_min.setValue(min.msec());
}

void CSPTimeRelation::onMaxDurationChanged(const TimeValue& max)
{
    if(max.isInfinite())
    {
        //TODO : ??? remove constraints on max?
    }else
    {
        m_max.setValue(max.msec());
    }
}

void CSPTimeRelation::onProcessCreated(const Process& process)
{
    if(auto scenario = dynamic_cast<const Scenario::ScenarioModel*>(&process))
    {
        m_subScenarios.insert(scenario->id(), new CSPScenario(*scenario, const_cast<Scenario::ScenarioModel*>(scenario)));
    }
}

void CSPTimeRelation::onProcessRemoved(const Process& process)
{
    if(auto scenario = dynamic_cast<const Scenario::ScenarioModel*>(&process))
    {
        delete(m_subScenarios.take(scenario->id()));
    }
}

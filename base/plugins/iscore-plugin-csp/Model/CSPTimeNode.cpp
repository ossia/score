#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <kiwi/kiwi.h>
#include <Process/ScenarioInterface.hpp>


CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId)
    :CSPConstraintHolder::CSPConstraintHolder(&cspScenario)
{
    auto& solver = cspScenario.getSolver();

    this->setParent(&cspScenario);
    this->setObjectName("CSPTimeNode");

    auto& timeNodeModel = cspScenario.getScenario()->timeNode(timeNodeId);

    m_iscoreDate = &timeNodeModel.date();

    m_date.setValue(m_iscoreDate->msec());

    //weight
    solver.addEditVariable(m_date, kiwi::strength::weak);

    // apply model constraints

    // 1 - events cannot happen before the start node
    // except for start timenode
    if(timeNodeId.val() != 0)
    {
        kiwi::Constraint* constraintNodeAfterStart = new kiwi::Constraint(m_date >= cspScenario.getStartTimeNode()->getDate());

        solver.addConstraint(*constraintNodeAfterStart);

        m_constraints.push_back(constraintNodeAfterStart);
    }

    // watch over date edits
    con(timeNodeModel, &TimeNodeModel::dateChanged, this, &CSPTimeNode::onDateChanged);
}

kiwi::Variable& CSPTimeNode::getDate()
{
    return m_date;
}

bool CSPTimeNode::dateChanged() const
{
    return m_date.value() != m_iscoreDate->msec();
}

void CSPTimeNode::onDateChanged(const TimeValue& date)
{
    m_date.setValue(date.msec());
}

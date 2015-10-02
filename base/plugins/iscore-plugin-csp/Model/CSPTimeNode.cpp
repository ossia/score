#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <kiwi/kiwi.h>
#include <Process/ScenarioInterface.hpp>


CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId)
    :CSPConstraintHolder::CSPConstraintHolder(cspScenario.getSolver(), &cspScenario)
{
    auto& solver = cspScenario.getSolver();

    this->setParent(&cspScenario);
    this->setObjectName("CSPTimeNode");

    auto& timeNodeModel = cspScenario.getScenario()->timeNode(timeNodeId);

    m_iscoreDate = &timeNodeModel.date();

    m_date.setValue(m_iscoreDate->msec());

    // apply model constraints

    // 1 - events cannot happen before the start node
    // except for start timenode
    if(timeNodeId.val() != 0)
    {
        PUT_CONSTRAINT(constraintNodeAfterStart, m_date >= cspScenario.getStartTimeNode()->getDate());
    }else// if it is indeed start node, constrain him the the start value
    {
        PUT_CONSTRAINT(cStartDontMove, m_date == m_date.value());
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

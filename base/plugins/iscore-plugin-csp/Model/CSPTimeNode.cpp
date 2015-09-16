#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <kiwi/kiwi.h>
#include <Process/ScenarioInterface.hpp>


CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId)
    :CSPConstraintHolder::CSPConstraintHolder(&cspScenario)
{
    this->setParent(&cspScenario);
    this->setObjectName("CSPTimeNode");

    auto& timeNodeModel = cspScenario.getScenario()->timeNode(timeNodeId);

    m_date.setValue(timeNodeModel.date().msec());

    // apply model constraints

    // 1 - events cannot happen before the start node
    // except for start timenode
    if(timeNodeId.val() != 0)
    {
        kiwi::Constraint* constraintNodeAfterStart = new kiwi::Constraint(m_date >= cspScenario.getStartTimeNode()->getDate());

        cspScenario.getSolver().addConstraint(*constraintNodeAfterStart);

        m_constraints.push_back(constraintNodeAfterStart);
    }

    // watch over date edits
    con(timeNodeModel, &TimeNodeModel::dateChanged, this, &CSPTimeNode::onDateChanged);
}

const kiwi::Variable& CSPTimeNode::getDate() const
{
    return m_date;
}

void CSPTimeNode::onDateChanged(const TimeValue& date)
{
    m_date.setValue(date.msec());
}

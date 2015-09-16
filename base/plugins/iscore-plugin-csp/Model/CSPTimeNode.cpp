#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <kiwi/kiwi.h>
#include <Process/ScenarioInterface.hpp>


CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId)
{
    auto& timeNodeModel = cspScenario.getScenario()->timeNode(timeNodeId);

    m_date.setValue(timeNodeModel.date().msec());

    // apply model constraints

    // 1 - events cannot happen before the start node
    // except for start timenode
    if(timeNodeId.val() != 0)
    {
        cspScenario.getSolver().addConstraint(m_date >= cspScenario.getStartTimeNode()->getDate());
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

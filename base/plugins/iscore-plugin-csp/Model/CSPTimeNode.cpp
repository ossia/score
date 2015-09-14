#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <Process/ScenarioInterface.hpp>

CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId)
    :m_date(cspScenario.getScenario()->timeNode(timeNodeId).date().msec())
{
    // apply model constraints

    // 1 - events cannot happen before the start node
    // except for start timenode
    if(timeNodeId.val() != 0)
    {
        cspScenario.getSolver().add_constraints(
        {
                       m_date >= cspScenario.getStartTimeNode()->getDate()
                    });
    }
}

const rhea::variable& CSPTimeNode::getDate() const
{
    return m_date;
}

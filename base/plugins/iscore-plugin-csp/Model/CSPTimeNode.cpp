#include "CSPTimeNode.hpp"
#include "CSPScenario.hpp"

#include <Process/ScenarioInterface.hpp>

CSPTimeNode::CSPTimeNode(CSPScenario& cspScenario, const TimeNodeModel& timeNodeModel)
{
    // copy attributes values
    m_date.change_value(timeNodeModel.date().msec());

    // apply model constraints

    // 1 - events cannot happen before the start node
    cspScenario.getSolver().add_constraints(
    {
                   m_date >= cspScenario.getStartTimeNode()->getDate()
                });
}

rhea::variable CSPTimeNode::getDate() const
{
    return m_date;
}

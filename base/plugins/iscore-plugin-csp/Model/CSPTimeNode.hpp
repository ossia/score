#pragma once

#include <ProcessInterface/TimeValue.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <rhea/simplex_solver.hpp>

class CSPScenario;
class TimeNodeModel;

class CSPTimeNode
{
public:
    CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId);

    CSPTimeNode() = default;

    const rhea::variable& getDate() const;

private:
    rhea::variable m_date;
};

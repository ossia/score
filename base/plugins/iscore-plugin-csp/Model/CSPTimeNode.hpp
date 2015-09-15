#pragma once

#include <ProcessInterface/TimeValue.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <kiwi/kiwi.h>

class CSPScenario;
class TimeNodeModel;

class CSPTimeNode
{
public:
    CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId);

    CSPTimeNode() = default;

    const kiwi::Variable& getDate() const;

private:
    kiwi::Variable m_date{"date"};
};

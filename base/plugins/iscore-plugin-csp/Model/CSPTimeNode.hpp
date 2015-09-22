#pragma once

#include <ProcessInterface/TimeValue.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Model/tools/CSPConstraintHolder.hpp>

#include <kiwi/kiwi.h>
#include <QVector>

#include "CSPScenario.hpp"

class TimeNodeModel;

class CSPTimeNode : public CSPConstraintHolder
{
public:
    CSPTimeNode(CSPScenario& cspScenario, const Id<TimeNodeModel>& timeNodeId);

    CSPTimeNode() = default;

    ~CSPTimeNode() = default;

    const kiwi::Variable& getDate() const;

private:
    kiwi::Variable m_date{"date"};

    void onDateChanged(const TimeValue& date);
};

#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Model/tools/CSPConstraintHolder.hpp>

#include <kiwi/kiwi.h>
#include <QVector>

#include "CSPScenario.hpp"

class CSPDisplacementPolicy;

namespace Scenario
{
class TimeNodeModel;
}

class CSPTimeNode : public CSPConstraintHolder
{
    friend class CSPDisplacementPolicy;
    friend class CSPFlexDisplacementPolicy;

public:
    CSPTimeNode(CSPScenario& cspScenario, const Id<Scenario::TimeNodeModel>& timeNodeId);

    CSPTimeNode() = default;

    ~CSPTimeNode() = default;

    kiwi::Variable& getDate();

    /**
     * @brief dateChanged
     * call this function to check if csp date differ from iscore date
     * @return
     */
    bool dateChanged() const;

private:
    kiwi::Variable m_date{"date"};
    const TimeValue* m_iscoreDate;

    void onDateChanged(const TimeValue& date);
};

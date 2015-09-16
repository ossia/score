#pragma once

#include <kiwi/kiwi.h>
#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>


class CSPScenario;

class ConstraintModel;

class CSPTimeRelation : public QObject
{

public:
    CSPTimeRelation(CSPScenario& scenario, const Id<ConstraintModel>& constraintId);

    CSPTimeRelation() = default;

    const kiwi::Variable& getMin() const;

    const kiwi::Variable& getMax() const;

private:
    kiwi::Variable m_min{"min"};
    kiwi::Variable m_max{"max"};

    //void onDefaultDurationChanged(const TimeValue& arg);
    void onMinDurationChanged(const TimeValue& min);
    void onMaxDurationChanged(const TimeValue& max);

    QVector<CSPScenario*> m_subScenarios;
};

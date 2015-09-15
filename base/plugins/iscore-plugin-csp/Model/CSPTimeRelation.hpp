#pragma once

#include <kiwi/kiwi.h>
#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
class CSPScenario;

class ConstraintModel;

class CSPTimeRelation
{

public:
    CSPTimeRelation(CSPScenario& scenario, const Id<ConstraintModel>& constraintId);

    CSPTimeRelation() = default;

    const kiwi::Variable& getMin() const;

    const kiwi::Variable& getMax() const;

private:
    kiwi::Variable m_min{"min"};
    kiwi::Variable m_max{"max"};

    QVector<CSPScenario*> m_subScenarios;
};

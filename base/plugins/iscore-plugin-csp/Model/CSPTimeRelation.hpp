#pragma once

#include <rhea/simplex_solver.hpp>
#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
class CSPScenario;

class ConstraintModel;

class CSPTimeRelation
{

public:
    CSPTimeRelation(CSPScenario& scenario, const Id<ConstraintModel>& constraintId);

    CSPTimeRelation() = default;

    const rhea::variable& getMin() const;

    const rhea::variable& getMax() const;

private:
    rhea::variable m_min;
    rhea::variable m_max;

    QVector<CSPScenario*> m_subScenarios;
};

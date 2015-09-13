#pragma once

#include <rhea/simplex_solver.hpp>
#include <QVector>
class CSPScenario;

class ConstraintModel;

class CSPTimeRelation
{

public:
    CSPTimeRelation(CSPScenario& scenario, const ConstraintModel& constraint);

    CSPTimeRelation() = default;

    rhea::variable getMin() const;

    rhea::variable getMax() const;

private:
    rhea::variable m_min;
    rhea::variable m_max;

    QVector<CSPScenario*> m_subScenarios;
};

#pragma once

#include <rhea/simplex_solver.hpp>
#include <QVector>
class CSPScenario;

class ConstraintModel;

class CSPTimeRelation
{

public:
    CSPTimeRelation(CSPScenario& scenario, const ConstraintModel& constraint);
private:
    rhea::variable m_min;
    rhea::variable m_max;

    QVector<CSPScenario> m_subScenarios;
};

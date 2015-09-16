#pragma once

#include <QObject>
#include <QVector>
#include <Model/CSPScenario.hpp>
#include <kiwi/kiwi.h>

class CSPConstraintHolder : public QObject
{
public:

    using QObject::QObject;

    ~CSPConstraintHolder()
    {
        CSPScenario* cspScenario = static_cast<CSPScenario*>(parent());

        for(auto constraint : m_constraints)
        {
           cspScenario->getSolver().removeConstraint(*constraint);
           delete constraint;
        }
    }
protected:
    QVector<kiwi::Constraint*> m_constraints;
};

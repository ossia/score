#pragma once

#include <QObject>
#include <QVector>
#include <Model/CSPScenario.hpp>
#include <kiwi/kiwi.h>

class CSPConstraintHolder : public QObject
{
public:

    using QObject::QObject;

    virtual ~CSPConstraintHolder()
    {
        removeAllConstraints();
    }

    void removeAllConstraints()
    {
        CSPScenario* cspScenario = static_cast<CSPScenario*>(parent());

        for(auto constraint : m_constraints)
        {
           cspScenario->getSolver().removeConstraint(*constraint);
           delete constraint;
        }

        removeStays();
    }

    void addStay(kiwi::Constraint* stay)
    {
        CSPScenario* cspScenario = static_cast<CSPScenario*>(parent());

        cspScenario->getSolver().addConstraint(*stay);
        m_stays.push_back(stay);
    }

    void removeStays()
    {
        CSPScenario* cspScenario = static_cast<CSPScenario*>(parent());

        for(auto stay : m_stays)
        {
           cspScenario->getSolver().removeConstraint(*stay);
           delete stay;
        }

        m_stays.clear();//important
    }

protected:
    QVector<kiwi::Constraint*> m_constraints;
    QVector<kiwi::Constraint*> m_stays;
};

#pragma once

#include <kiwi/kiwi.h>
#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <Model/tools/CSPConstraintHolder.hpp>

#include <Model/CSPScenario.hpp>

class CSPDisplacementPolicy;

class ConstraintModel;

class CSPTimeRelation : public CSPConstraintHolder
{
    friend class CSPDisplacementPolicy;
public:
    CSPTimeRelation(CSPScenario& scenario, const Id<ConstraintModel>& constraintId);

    CSPTimeRelation() = default;

    ~CSPTimeRelation();

    kiwi::Variable& getMin();

    kiwi::Variable& getMax();

    /**
     * @brief minChanged
     * call this function to check if csp min differ from iscore min
     * @return
     */
    bool minChanged() const;

    /**
     * @brief maxChanged
     * call this function to check if csp max differ from iscore max
     * @return
     */
    bool maxChanged() const;

private:
    kiwi::Solver& m_solver;

    kiwi::Variable m_min{"min"};
    const TimeValue* m_iscoreMin;
    kiwi::Variable m_max{"max"};
    const TimeValue* m_iscoreMax;

    //void onDefaultDurationChanged(const TimeValue& arg);
    void onMinDurationChanged(const TimeValue& min);
    void onMaxDurationChanged(const TimeValue& max);

    QVector<CSPScenario*> m_subScenarios;
};

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
    kiwi::Variable m_min{"min"};
    const TimeValue* m_iscoreMin;

    kiwi::Variable m_max{"max"};
    const TimeValue* m_iscoreMax;

    kiwi::Constraint m_cstrRigidity{kiwi::Constraint(m_min == m_max)};// TODO ask JM if it is safe to do so

    //void onDefaultDurationChanged(const TimeValue& arg);
    void onMinDurationChanged(const TimeValue& min);
    void onMaxDurationChanged(const TimeValue& max);

    void onProcessCreated(const Process& process);
    void onProcessRemoved(const Process& process);

    QHash<Id<Process>, CSPScenario*> m_subScenarios;
};

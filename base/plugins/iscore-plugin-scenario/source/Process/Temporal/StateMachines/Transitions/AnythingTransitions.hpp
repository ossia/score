#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class MoveOnAnything_Transition : public GenericTransition<QAbstractTransition>
{
    public:
        using GenericTransition<QAbstractTransition>::GenericTransition;

    protected:
        bool eventTest(QEvent *e) override;
        void onTransition(QEvent *event) override;
};

class ReleaseOnAnything_Transition : public QAbstractTransition
{
    protected:
        bool eventTest(QEvent *e) override;
        void onTransition(QEvent *event) override;
};

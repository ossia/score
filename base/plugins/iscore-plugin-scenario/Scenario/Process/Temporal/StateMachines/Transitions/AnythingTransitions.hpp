#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>
namespace Scenario
{
class MoveOnAnything_Transition final : public GenericTransition<QAbstractTransition>
{
    public:
        using GenericTransition<QAbstractTransition>::GenericTransition;

    protected:
        bool eventTest(QEvent *e) override;
        void onTransition(QEvent *event) override;
};

class ReleaseOnAnything_Transition final : public QAbstractTransition
{
    protected:
        bool eventTest(QEvent *e) override;
        void onTransition(QEvent *event) override;
};
}

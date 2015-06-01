#pragma once
#include "Process/Temporal/StateMachines/Tools/ScenarioToolState.hpp"
#include <iscore/selection/SelectionDispatcher.hpp>

class SelectionToolState : public ScenarioToolState
{
    public:
        SelectionToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

        void setSelectionArea(const QRectF& area);


    private:
        QState* m_singleSelection{};
        QState* m_multiSelection{};

        iscore::SelectionDispatcher m_dispatcher;

        QPointF m_initialPoint;
        QPointF m_movePoint;

        QState* m_waitState{};
};

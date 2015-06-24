#pragma once

#include <iscore/statemachine/CommonSelectionState.hpp>

#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"



class ScenarioSelectionState : public CommonSelectionState
{
    public:
        ScenarioSelectionState(
                iscore::SelectionStack& stack,
                const ScenarioStateMachine& parentSM,
                TemporalScenarioView& scenarioview,
                QState* parent);

        const QPointF& initialPoint() const
        { return m_initialPoint; }
        const QPointF& movePoint() const
        { return m_movePoint; }

        void on_pressAreaSelection() override;
        void on_moveAreaSelection() override;
        void on_releaseAreaSelection() override;

         void on_deselect() override;
         void on_delete() override;
         void on_deleteContent() override;

         void setSelectionArea(const QRectF& area);


    private:
        QPointF m_initialPoint;
        QPointF m_movePoint;
        const ScenarioStateMachine& m_parentSM;
        TemporalScenarioView& m_scenarioView;
};


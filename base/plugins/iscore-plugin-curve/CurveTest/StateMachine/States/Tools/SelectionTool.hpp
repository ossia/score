#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>

class QState;
namespace Curve
{
class SelectionTool : public QState
{
    public:
        SelectionTool(ScenarioStateMachine& sm);

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
}

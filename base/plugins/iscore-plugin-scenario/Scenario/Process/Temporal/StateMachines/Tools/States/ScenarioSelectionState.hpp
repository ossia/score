#pragma once
#include <iscore/statemachine/CommonSelectionState.hpp>
#include <QPointF>
class TemporalScenarioView;

namespace Scenario
{
class ToolPalette;
class SelectionState final : public CommonSelectionState
{
    private:
        QPointF m_initialPoint;
        QPointF m_movePoint;
        const Scenario::ToolPalette& m_parentSM;
        TemporalScenarioView& m_scenarioView;

    public:
        SelectionState(
                iscore::SelectionStack& stack,
                const Scenario::ToolPalette& parentSM,
                TemporalScenarioView& scenarioview,
                QState* parent);

        const QPointF& initialPoint() const;
        const QPointF& movePoint() const;

        void on_pressAreaSelection() override;

        void on_moveAreaSelection() override;

        void on_releaseAreaSelection() override;

        void on_deselect() override;

        void on_delete() override;

        void on_deleteContent() override;

        void setSelectionArea(const QRectF& area);
};
}

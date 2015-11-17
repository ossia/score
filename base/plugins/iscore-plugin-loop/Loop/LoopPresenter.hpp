#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Process/LayerPresenter.hpp>

#include <core/document/DocumentContext.hpp>
#include <Loop/LoopViewUpdater.hpp>

class LoopLayer;
class LoopView;
class LoopPresenter : public LayerPresenter
{
        friend class LoopViewUpdater;
    public:
        LoopPresenter(
                iscore::DocumentContext& context,
                const LoopLayer&,
                LoopView* view,
                QObject* parent);

        ~LoopPresenter();

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const LayerModel& layerModel() const override;
        const Id<Process>& modelId() const override;

        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;

    private:
        const LoopLayer& m_layer;
        LoopView* m_view{};

        ZoomRatio m_zoomRatio {};

        StatePresenter* m_startState{};
        StatePresenter* m_endState{};
        EventPresenter* m_startEvent{};
        EventPresenter* m_endEvent{};
        TimeNodePresenter* m_startNode{};
        TimeNodePresenter* m_endNode{};
        TemporalConstraintPresenter* m_constraint{};

        LoopViewUpdater m_viewUpdater;
        void updateAllElements();
};

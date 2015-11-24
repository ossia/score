#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenarioPresenter.hpp>
#include <Process/LayerPresenter.hpp>

#include <core/document/DocumentContext.hpp>
#include <Loop/LoopViewUpdater.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>


namespace Loop
{
inline void removeSelection(const Loop::ProcessModel& model, iscore::CommandStack& )
{

}

inline void clearContentFromSelection(const Loop::ProcessModel& model, iscore::CommandStack& )
{
    ISCORE_TODO;
}
}

class LoopLayer;
class LoopView;
class LoopPresenter :
        public LayerPresenter,
        public BaseScenarioPresenter<Loop::ProcessModel, TemporalConstraintPresenter>
{
        Q_OBJECT
        friend class LoopViewUpdater;
    public:
        LoopPresenter(
                iscore::DocumentContext& context,
                const LoopLayer&,
                LoopView* view,
                QObject* parent);

        ~LoopPresenter();

        using  BaseScenarioPresenter<Loop::ProcessModel, TemporalConstraintPresenter>::event;

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const LayerModel& layerModel() const override;
        const Id<Process>& modelId() const override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;

    signals:
        void pressed(QPointF);
        void moved(QPointF);
        void released(QPointF);
        void escPressed();

    private:
        const LoopLayer& m_layer;
        LoopView* m_view{};

        ZoomRatio m_zoomRatio {};

        LoopViewUpdater m_viewUpdater;

        FocusDispatcher m_focusDispatcher;
        LayerContext m_context;
        LoopToolPalette m_palette;
        void updateAllElements();
};

#include "LoopPresenter.hpp"
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopLayer.hpp>
#include <Loop/LoopView.hpp>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>


#include <iscore/widgets/GraphicsItem.hpp>



LoopPresenter::LoopPresenter(
        iscore::DocumentContext& context,
        const LoopLayer& layer,
        LoopView* view,
        QObject* parent):
    LayerPresenter{"LoopPresenter", parent},
    m_layer{layer},
    m_view{view},
    m_viewUpdater{*this}
{
    m_constraint = new TemporalConstraintPresenter{
            layer.constraint(), view, this};
    m_startState = new StatePresenter{
            layer.model().BaseScenarioContainer::startState(), m_view, this};
    m_endState = new StatePresenter{
            layer.model().BaseScenarioContainer::endState(), m_view, this};
    m_startEvent= new EventPresenter{
            layer.model().startEvent(), m_view, this};
    m_endEvent = new EventPresenter{
            layer.model().endEvent(), m_view, this};
    m_startNode = new TimeNodePresenter{
            layer.model().startTimeNode(), m_view, this};
    m_endNode = new TimeNodePresenter{
            layer.model().endTimeNode(), m_view, this};
}

LoopPresenter::~LoopPresenter()
{
    deleteGraphicsObject(m_view);
}

void LoopPresenter::setWidth(int width)
{
    m_view->setWidth(width);
}

void LoopPresenter::setHeight(int height)
{
    m_view->setHeight(height);
}

void LoopPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    m_view->setOpacity(1);
}

void LoopPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    m_view->setOpacity(0.1);
}

void LoopPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;
    m_constraint->on_zoomRatioChanged(m_zoomRatio);
}

void LoopPresenter::parentGeometryChanged()
{
    updateAllElements();
    m_view->update();
}

const LayerModel& LoopPresenter::layerModel() const
{
    return m_layer;
}

const Id<Process>&LoopPresenter::modelId() const
{
    return m_layer.model().id();
}

void LoopPresenter::updateAllElements()
{
    m_viewUpdater.updateConstraint(*m_constraint);
    m_viewUpdater.updateEvent(*m_startEvent);
    m_viewUpdater.updateEvent(*m_endEvent);
    m_viewUpdater.updateTimeNode(*m_startNode);
    m_viewUpdater.updateTimeNode(*m_endNode);
}

void LoopPresenter::fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const
{
}



#include <Loop/LoopLayer.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <QGraphicsItem>
#include <tuple>
#include <type_traits>

#include "Loop/LoopViewUpdater.hpp"
#include "LoopPresenter.hpp"
#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/tools/std/Algorithms.hpp>

class LayerModel;
class Process;
class QMenu;
class QObject;
struct VerticalExtent;


LoopPresenter::LoopPresenter(
        const iscore::DocumentContext& context,
        const LoopLayer& layer,
        LoopView* view,
        QObject* parent):
    LayerPresenter{"LoopPresenter", parent},
    BaseScenarioPresenter<Loop::ProcessModel, TemporalConstraintPresenter>{layer.model()},
    m_layer{layer},
    m_view{view},
    m_viewUpdater{*this},
    m_focusDispatcher{context.document},
    m_context{context, *this, m_focusDispatcher},
    m_palette{m_layer.model(), *this, m_context, *m_view}
{
    m_constraintPresenter = new TemporalConstraintPresenter{
            layer.constraint(), view, this};
    m_startStatePresenter = new StatePresenter{
            layer.model().BaseScenarioContainer::startState(), m_view, this};
    m_endStatePresenter = new StatePresenter{
            layer.model().BaseScenarioContainer::endState(), m_view, this};
    m_startEventPresenter = new EventPresenter{
            layer.model().startEvent(), m_view, this};
    m_endEventPresenter = new EventPresenter{
            layer.model().endEvent(), m_view, this};
    m_startNodePresenter = new TimeNodePresenter{
            layer.model().startTimeNode(), m_view, this};
    m_endNodePresenter = new TimeNodePresenter{
            layer.model().endTimeNode(), m_view, this};


    auto elements = std::make_tuple(
                m_constraintPresenter,
                m_startStatePresenter,
                m_endStatePresenter,
                m_startEventPresenter,
                m_endEventPresenter,
                m_startNodePresenter,
                m_endNodePresenter);

    for_each_in_tuple(elements, [&] (auto elt) {
        using elt_t = std::remove_reference_t<decltype(*elt)>;
        connect(elt, &elt_t::pressed,  this, &LoopPresenter::pressed);
        connect(elt, &elt_t::moved,    this, &LoopPresenter::moved);
        connect(elt, &elt_t::released, this, &LoopPresenter::released);
    });

    con(m_endEventPresenter->model(), &EventModel::extentChanged,
        this, [=] (const VerticalExtent&) { m_viewUpdater.updateEvent(*m_endEventPresenter); });
    con(m_endEventPresenter->model(), &EventModel::dateChanged,
            this, [=] (const TimeValue&) { m_viewUpdater.updateEvent(*m_endEventPresenter); });
    con(m_endNodePresenter->model(), &TimeNodeModel::extentChanged,
        this, [=] (const VerticalExtent&) { m_viewUpdater.updateTimeNode(*m_endNodePresenter); });
    con(m_endNodePresenter->model(), &TimeNodeModel::dateChanged,
        this, [=] (const TimeValue&) { m_viewUpdater.updateTimeNode(*m_endNodePresenter); });
}

LoopPresenter::~LoopPresenter()
{
    deleteGraphicsObject(m_view);
}

void LoopPresenter::setWidth(qreal width)
{
    m_view->setWidth(width);
}

void LoopPresenter::setHeight(qreal height)
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
    m_constraintPresenter->on_zoomRatioChanged(m_zoomRatio);
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
    m_viewUpdater.updateConstraint(*m_constraintPresenter);
    m_viewUpdater.updateEvent(*m_startEventPresenter);
    m_viewUpdater.updateEvent(*m_endEventPresenter);
    m_viewUpdater.updateTimeNode(*m_startNodePresenter);
    m_viewUpdater.updateTimeNode(*m_endNodePresenter);
}

void LoopPresenter::fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const
{
}



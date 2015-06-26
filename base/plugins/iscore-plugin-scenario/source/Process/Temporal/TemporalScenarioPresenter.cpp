#include "TemporalScenarioPresenter.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "ScenarioViewInterface.hpp"

#include <QGraphicsScene>

TemporalScenarioPresenter::TemporalScenarioPresenter(
        const TemporalScenarioViewModel& process_view_model,
        Layer* view,
        QObject* parent) :
    ProcessPresenter {"TemporalScenarioPresenter", parent},
    m_viewModel {process_view_model},
    m_view {static_cast<TemporalScenarioView*>(view)},
    m_viewInterface{new ScenarioViewInterface{this}},
    m_sm{*this},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel.sharedProcessModel())}
{
    const auto& scenario = model(m_viewModel);
    /////// Setup of existing data
    // For each constraint & event, display' em
    for(const auto& event_model : scenario.events())
    {
        on_eventCreated_impl(*event_model);
    }

    for(const auto& tn_model : scenario.timeNodes())
    {
        on_timeNodeCreated_impl(*tn_model);
    }

    for(const auto& constraint_view_model : constraintsViewModels(m_viewModel))
    {
        on_constraintCreated_impl(*constraint_view_model);
    }


    /////// Connections
    connect(&m_viewModel, &TemporalScenarioViewModel::eventCreated,
            this,		 &TemporalScenarioPresenter::on_eventCreated);
    connect(&m_viewModel, &TemporalScenarioViewModel::eventDeleted,
            this,		 &TemporalScenarioPresenter::on_eventRemoved);

    connect(&m_viewModel, &TemporalScenarioViewModel::timeNodeCreated,
            this,        &TemporalScenarioPresenter::on_timeNodeCreated);
    connect(&m_viewModel, &TemporalScenarioViewModel::timeNodeDeleted,
            this,        &TemporalScenarioPresenter::on_timeNodeRemoved);

    connect(&m_viewModel, &TemporalScenarioViewModel::constraintViewModelCreated,
            this,		 &TemporalScenarioPresenter::on_constraintCreated);
    connect(&m_viewModel, &TemporalScenarioViewModel::constraintViewModelRemoved,
            this,		 &TemporalScenarioPresenter::on_constraintViewModelRemoved);

    connect(m_view, &TemporalScenarioView::scenarioPressed,
            this, [&] (const QPointF&)
    {
        m_focusDispatcher.focus(this);
    });

    connect(m_view, &TemporalScenarioView::keyPressed,
            this,   &TemporalScenarioPresenter::keyPressed);
    connect(m_view, &TemporalScenarioView::keyReleased,
            this,   &TemporalScenarioPresenter::keyReleased);

    connect(m_view, &TemporalScenarioView::askContextMenu,
            this,   &TemporalScenarioPresenter::contextMenuAsked);


    connect(&model(m_viewModel), &ScenarioModel::locked,
            m_view,             &TemporalScenarioView::lock);
    connect(&model(m_viewModel), &ScenarioModel::unlocked,
            m_view,             &TemporalScenarioView::unlock);

    m_sm.start();
}

TemporalScenarioPresenter::~TemporalScenarioPresenter()
{
    delete m_viewInterface;

    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

const LayerModel& TemporalScenarioPresenter::viewModel() const
{
    return m_viewModel;
}

const id_type<ProcessModel>& TemporalScenarioPresenter::modelId() const
{
    return m_viewModel.sharedProcessModel().id();
}

void TemporalScenarioPresenter::setWidth(int width)
{
    m_view->setWidth(width);
}

void TemporalScenarioPresenter::setHeight(int height)
{
    m_view->setHeight(height);

    // Move all the elements accordingly
    for(auto& constraint : m_constraints)
    {
        m_viewInterface->on_constraintMoved(constraint->abstractConstraintViewModel().model().id());
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(event->id());
    }

}

void TemporalScenarioPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    m_view->setOpacity(1);
}

void TemporalScenarioPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    m_view->setOpacity(0.1);
}

void TemporalScenarioPresenter::parentGeometryChanged()
{
    m_view->update();
    // Move all the elements accordingly
    for(auto& constraint : m_constraints)
    {
        m_viewInterface->on_constraintMoved(constraint->abstractConstraintViewModel().model().id());
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(event->id());
    }
}

void TemporalScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(auto& constraint : m_constraints)
    {
        constraint->on_zoomRatioChanged(m_zoomRatio);
        m_viewInterface->on_constraintMoved(constraint->abstractConstraintViewModel().model().id());
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(event->id());
    }
}

void TemporalScenarioPresenter::on_eventCreated(
        const id_type<EventModel>& eventId)
{
    on_eventCreated_impl(model(m_viewModel).event(eventId));
}

void TemporalScenarioPresenter::on_timeNodeCreated(
        const id_type<TimeNodeModel>& timeNodeId)
{
    on_timeNodeCreated_impl(model(m_viewModel).timeNode(timeNodeId));
}

void TemporalScenarioPresenter::on_constraintCreated(
        const id_type<AbstractConstraintViewModel>& constraintViewModelId)
{
    on_constraintCreated_impl(constraintViewModel(m_viewModel, constraintViewModelId));
}

void TemporalScenarioPresenter::on_eventRemoved(
        const id_type<EventModel>& eventId)
{
    auto ev = m_events.at(eventId);

    // Get the timenode to update
    auto tn_id = ev->model().timeNode();

    // Remove the event
    delete ev;
    m_events.remove(eventId);

    m_viewInterface->updateTimeNode(tn_id);
    m_view->update();
}

void TemporalScenarioPresenter::on_timeNodeRemoved(
        const id_type<TimeNodeModel>& timeNodeId)
{
    delete m_timeNodes.at(timeNodeId);
    m_timeNodes.remove(timeNodeId);

    m_view->update();
}

void TemporalScenarioPresenter::on_constraintViewModelRemoved(
        const id_type<AbstractConstraintViewModel>& constraintViewModelId)
{
    for(const auto& pres : m_constraints)
    {
        // TODO add an index in the map on viewmodel id ?
        if(::viewModel(pres)->id() == constraintViewModelId)
        {
            auto cid = pres->id();
            auto& cm = pres->model();
            auto& stn = m_events.at(cm.startEvent())->model().timeNode();
            auto& etn = m_events.at(cm.endEvent())->model().timeNode();

            delete pres;
            m_constraints.remove(cid);

            m_viewInterface->updateTimeNode(stn);
            m_viewInterface->updateTimeNode(etn);
            break;
        }
    }

    m_view->update();
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void TemporalScenarioPresenter::on_askUpdate()
{
    m_view->update();
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void TemporalScenarioPresenter::on_eventCreated_impl(const EventModel& event_model)
{
    auto rect = m_view->boundingRect();

    auto ev_pres = new EventPresenter {event_model,
                           m_view,
                           this};
    m_events.insert(ev_pres);

    ev_pres->view()
           ->setPos({rect.x() + event_model.date().toPixels(m_zoomRatio),
                     rect.y() + rect.height() * event_model.heightPercentage() });

    connect(ev_pres, &EventPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(ev_pres, &EventPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(ev_pres, &EventPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);

    connect(&event_model, &EventModel::previousConstraintsChanged,
            this, [&] ()
    {
        auto& timenode = static_cast<ScenarioModel&>(viewModel().sharedProcessModel()).timeNode(event_model.timeNode());
        timenode.previousConstraintsChanged(
                    event_model.id(),
                    !event_model.previousConstraints().empty());
    });


    connect(ev_pres, &EventPresenter::eventHoverEnter,
            this, [=] ()
    { m_viewInterface->on_hoverOnEvent(ev_pres->id(), true); });

    connect(ev_pres, &EventPresenter::eventHoverLeave,
            this, [=] ()
    { m_viewInterface->on_hoverOnEvent(ev_pres->id(), false); });

}

void TemporalScenarioPresenter::on_timeNodeCreated_impl(const TimeNodeModel& timeNode_model)
{
    auto rect = m_view->boundingRect();

    auto tn_pres = new TimeNodePresenter {
            timeNode_model,
            m_view,
            this};
    m_timeNodes.insert(tn_pres);

    tn_pres->view()
           ->setPos({timeNode_model.date().toPixels(m_zoomRatio),
                     timeNode_model.y() * rect.height()});

    connect(tn_pres, &TimeNodePresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(tn_pres, &TimeNodePresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(tn_pres, &TimeNodePresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_constraintCreated_impl(const TemporalConstraintViewModel& constraint_view_model)
{
    auto rect = m_view->boundingRect();

    auto cst_pres = new TemporalConstraintPresenter{
                                constraint_view_model,
                                m_view,
                                this};
    cst_pres->view()->setConnections();
    m_constraints.insert(cst_pres);
    cst_pres->on_zoomRatioChanged(m_zoomRatio);

    cst_pres->view()->setPos({rect.x() + constraint_view_model.model().startDate().toPixels(m_zoomRatio),
                             rect.y() + rect.height() * constraint_view_model.model().heightPercentage() });



    m_viewInterface->addPointInEvent(constraint_view_model.model().endEvent(),
                                        constraint_view_model.model().heightPercentage());
    m_viewInterface->addPointInEvent(constraint_view_model.model().startEvent(),
                                        constraint_view_model.model().heightPercentage());

    connect(cst_pres, &TemporalConstraintPresenter::heightPercentageChanged,
            this, [=] ()
    {
        auto rect = m_view->boundingRect();
        const auto& cst = cst_pres->abstractConstraintViewModel().model();
        cst_pres->view()->setPos({rect.x() + cst.startDate().toPixels(m_zoomRatio),
                                  rect.y() + rect.height() * cst.heightPercentage() });
        m_viewInterface->updateTimeNode(m_events.at(cst.endEvent())->model().timeNode());
        m_viewInterface->updateTimeNode(m_events.at(cst.startEvent())->model().timeNode() );
    });

    connect(cst_pres, &TemporalConstraintPresenter::askUpdate,
            this,     &TemporalScenarioPresenter::on_askUpdate);

    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverEnter,
            [=] ()
    {
        m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), true);
    });
    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverLeave,
            [=] ()
    {
        m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), false);
    });

    connect(cst_pres, &TemporalConstraintPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(cst_pres, &TemporalConstraintPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(cst_pres, &TemporalConstraintPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

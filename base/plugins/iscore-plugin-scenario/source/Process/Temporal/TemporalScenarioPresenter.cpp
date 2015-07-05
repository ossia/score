#include "TemporalScenarioPresenter.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/State/DisplayedStateModel.hpp"

#include "ScenarioViewInterface.hpp"

#include <QGraphicsScene>

TemporalScenarioPresenter::TemporalScenarioPresenter(
        const TemporalScenarioLayer& process_view_model,
        Layer* view,
        QObject* parent) :
    ProcessPresenter {"TemporalScenarioPresenter", parent},
    m_layer {process_view_model},
    m_view {static_cast<TemporalScenarioView*>(view)},
    m_viewInterface{new ScenarioViewInterface{this}},
    m_sm{*this},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_layer.sharedProcessModel())}
{
    const ScenarioModel& scenario = model(m_layer);
    /////// Setup of existing data
    // For each constraint & event, display' em
    for(const auto& state_model : scenario.states())
    {
        on_stateCreated_impl(*state_model);
    }

    for(const auto& event_model : scenario.events())
    {
        on_eventCreated_impl(*event_model);
    }

    for(const auto& tn_model : scenario.timeNodes())
    {
        on_timeNodeCreated_impl(*tn_model);
    }

    for(const auto& constraint_view_model : constraintsViewModels(m_layer))
    {
        on_constraintCreated_impl(*constraint_view_model);
    }


    /////// Connections
    connect(&m_layer, &TemporalScenarioLayer::stateCreated,
            this,		 &TemporalScenarioPresenter::on_stateCreated);
    connect(&m_layer, &TemporalScenarioLayer::stateRemoved,
            this,		 &TemporalScenarioPresenter::on_stateRemoved);

    connect(&m_layer, &TemporalScenarioLayer::eventCreated,
            this,		 &TemporalScenarioPresenter::on_eventCreated);
    connect(&m_layer, &TemporalScenarioLayer::eventRemoved,
            this,		 &TemporalScenarioPresenter::on_eventRemoved);

    connect(&m_layer, &TemporalScenarioLayer::timeNodeCreated,
            this,        &TemporalScenarioPresenter::on_timeNodeCreated);
    connect(&m_layer, &TemporalScenarioLayer::timeNodeRemoved,
            this,        &TemporalScenarioPresenter::on_timeNodeRemoved);

    connect(&m_layer, &TemporalScenarioLayer::constraintViewModelCreated,
            this,		 &TemporalScenarioPresenter::on_constraintViewModelCreated);
    connect(&m_layer, &TemporalScenarioLayer::constraintViewModelRemoved,
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


    connect(&model(m_layer), &ScenarioModel::locked,
            m_view,             &TemporalScenarioView::lock);
    connect(&model(m_layer), &ScenarioModel::unlocked,
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
    return m_layer;
}

const id_type<ProcessModel>& TemporalScenarioPresenter::modelId() const
{
    return m_layer.sharedProcessModel().id();
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
        m_viewInterface->on_constraintMoved(*constraint);
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(*event);
    }

    for(auto& timenode : m_timeNodes)
    {
        m_viewInterface->on_timeNodeMoved(*timenode);
    }

    for(auto& state : m_displayedStates)
    {
        m_viewInterface->on_stateMoved(*state);
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
    // TODO refactor this ffs
    for(auto& constraint : m_constraints)
    {
        m_viewInterface->on_constraintMoved(*constraint);
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(*event);
    }

    for(auto& timenode : m_timeNodes)
    {
        m_viewInterface->on_timeNodeMoved(*timenode);
    }

    for(auto& state : m_displayedStates)
    {
        m_viewInterface->on_stateMoved(*state);
    }
}

void TemporalScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    // TODO maybe don't pass the id but directly the ptr.
    for(auto& constraint : m_constraints)
    {
        constraint->on_zoomRatioChanged(m_zoomRatio);
        m_viewInterface->on_constraintMoved(*constraint);
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(*event);
    }

    for(auto& timenode : m_timeNodes)
    {
        m_viewInterface->on_timeNodeMoved(*timenode);
    }

    for(auto& state : m_displayedStates)
    {
        m_viewInterface->on_stateMoved(*state);
    }
}


void TemporalScenarioPresenter::on_stateCreated(const id_type<StateModel> &stateId)
{
    on_stateCreated_impl(model(m_layer).displayedState(stateId));
}

void TemporalScenarioPresenter::on_eventCreated(
        const id_type<EventModel>& eventId)
{
    on_eventCreated_impl(model(m_layer).event(eventId));
}

void TemporalScenarioPresenter::on_timeNodeCreated(
        const id_type<TimeNodeModel>& timeNodeId)
{
    on_timeNodeCreated_impl(model(m_layer).timeNode(timeNodeId));
}

void TemporalScenarioPresenter::on_constraintViewModelCreated(
        const id_type<AbstractConstraintViewModel>& constraintViewModelId)
{
    on_constraintCreated_impl(constraintViewModel(m_layer, constraintViewModelId));
}

// TODO refactor these three ?
void TemporalScenarioPresenter::on_stateRemoved(const id_type<StateModel> &stateId)
{
    delete m_displayedStates.at(stateId);
    m_displayedStates.remove(stateId);

    m_view->update();
}


void TemporalScenarioPresenter::on_eventRemoved(
        const id_type<EventModel>& eventId)
{
    delete m_events.at(eventId);
    m_events.remove(eventId);

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
            m_constraints.remove(cid);
            delete pres;

            m_view->update();
            break;
        }
    }
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
    auto ev_pres = new EventPresenter {event_model,
                           m_view,
                           this};
    m_events.insert(ev_pres);

    m_viewInterface->on_eventMoved(*ev_pres);

    connect(&event_model, &EventModel::extentChanged,
            this, [=] (const VerticalExtent&) { m_viewInterface->on_eventMoved(*ev_pres); });
    connect(&event_model, &EventModel::dateChanged,
            this, [=] (const TimeValue&) { m_viewInterface->on_eventMoved(*ev_pres); });

    connect(ev_pres, &EventPresenter::eventHoverEnter,
            this, [=] () { m_viewInterface->on_hoverOnEvent(ev_pres->id(), true); });
    connect(ev_pres, &EventPresenter::eventHoverLeave,
            this, [=] () { m_viewInterface->on_hoverOnEvent(ev_pres->id(), false); });

    // For the state machine
    connect(ev_pres, &EventPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(ev_pres, &EventPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(ev_pres, &EventPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_timeNodeCreated_impl(const TimeNodeModel& timeNode_model)
{
    auto tn_pres = new TimeNodePresenter {timeNode_model, m_view, this};
    m_timeNodes.insert(tn_pres);

    m_viewInterface->on_timeNodeMoved(*tn_pres);

    connect(&timeNode_model, &TimeNodeModel::extentChanged,
            this, [=] (const VerticalExtent&) { m_viewInterface->on_timeNodeMoved(*tn_pres); });
    connect(&timeNode_model, &TimeNodeModel::dateChanged,
            this, [=] (const TimeValue&) { m_viewInterface->on_timeNodeMoved(*tn_pres); });

    // For the state machine
    connect(tn_pres, &TimeNodePresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(tn_pres, &TimeNodePresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(tn_pres, &TimeNodePresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_stateCreated_impl(const StateModel &state)
{
    auto st_pres = new StatePresenter{state, m_view, this};
    m_displayedStates.insert(st_pres);

    m_viewInterface->on_stateMoved(*st_pres);

    connect(&state, &StateModel::heightPercentageChanged,
            this, [=] () { m_viewInterface->on_stateMoved(*st_pres); });

    // For the state machine
    connect(st_pres, &StatePresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(st_pres, &StatePresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(st_pres, &StatePresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_constraintCreated_impl(const TemporalConstraintViewModel& constraint_view_model)
{
    auto cst_pres = new TemporalConstraintPresenter{
                                constraint_view_model,
                                m_view,
                                this};
    m_constraints.insert(cst_pres);
    cst_pres->on_zoomRatioChanged(m_zoomRatio);

    m_viewInterface->on_constraintMoved(*cst_pres);

    connect(cst_pres, &TemporalConstraintPresenter::heightPercentageChanged,
            this, [=] () { m_viewInterface->on_constraintMoved(*cst_pres); });
    connect(cst_pres, &TemporalConstraintPresenter::askUpdate,
            this,     &TemporalScenarioPresenter::on_askUpdate);

    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverEnter,
            [=] () { m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), true); });
    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverLeave,
            [=] () { m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), false); });

    // For the state machine
    connect(cst_pres, &TemporalConstraintPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(cst_pres, &TemporalConstraintPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(cst_pres, &TemporalConstraintPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

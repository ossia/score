#include "TemporalScenarioPresenter.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "ProcessInterface/ZoomHelper.hpp"

#include "ScenarioViewInterface.hpp"

#include <iscore/selection/Selection.hpp>
#include <iscore/tools/utilsCPP11.hpp>

#include <QDebug>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QStateMachine>
#include <QtMath>
#include "iscore/command/SerializableCommand.hpp"
#include <iscore/document/DocumentInterface.hpp>

TemporalScenarioPresenter::TemporalScenarioPresenter(TemporalScenarioViewModel* process_view_model,
                                                     ProcessViewInterface* view,
                                                     QObject* parent) :
    ProcessPresenterInterface {"TemporalScenarioPresenter", parent},
    m_viewModel {static_cast<TemporalScenarioViewModel*>(process_view_model) },
    m_view {static_cast<TemporalScenarioView*>(view) },
    m_viewInterface{new ScenarioViewInterface{this}},
    m_sm{*this},
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_viewModel->sharedProcessModel())}
{
    /////// Setup of existing data
    // For each constraint & event, display' em
    for(auto event_model : model(m_viewModel)->events())
    {
        on_eventCreated_impl(event_model);
    }

    for(auto tn_model : model(m_viewModel)->timeNodes())
    {
        on_timeNodeCreated_impl(tn_model);
    }

    for(auto constraint_view_model : constraintsViewModels(m_viewModel))
    {
        on_constraintCreated_impl(constraint_view_model);
    }

    /////// Connections
    connect(m_viewModel, &TemporalScenarioViewModel::eventCreated,
            this,		 &TemporalScenarioPresenter::on_eventCreated);
    connect(m_viewModel, &TemporalScenarioViewModel::eventDeleted,
            this,		 &TemporalScenarioPresenter::on_eventDeleted);

    connect(m_viewModel, &TemporalScenarioViewModel::timeNodeCreated,
            this,        &TemporalScenarioPresenter::on_timeNodeCreated);
    connect(m_viewModel, &TemporalScenarioViewModel::timeNodeDeleted,
            this,        &TemporalScenarioPresenter::on_timeNodeDeleted);

    connect(m_viewModel, &TemporalScenarioViewModel::constraintViewModelCreated,
            this,		 &TemporalScenarioPresenter::on_constraintCreated);
    connect(m_viewModel, &TemporalScenarioViewModel::constraintViewModelRemoved,
            this,		 &TemporalScenarioPresenter::on_constraintViewModelRemoved);

    connect(m_view, &TemporalScenarioView::scenarioPressed, [&] (const QPointF&)
    {
        m_focusDispatcher.focus(m_viewModel);
    });


    connect(model(m_viewModel), &ScenarioModel::locked,
            m_view,             &TemporalScenarioView::lock);
    connect(model(m_viewModel), &ScenarioModel::unlocked,
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

id_type<ProcessViewModelInterface> TemporalScenarioPresenter::viewModelId() const
{
    return m_viewModel->id();
}

id_type<ProcessSharedModelInterface> TemporalScenarioPresenter::modelId() const
{
    return m_viewModel->sharedProcessModel()->id();
}

void TemporalScenarioPresenter::setWidth(int width)
{
    m_view->setWidth(width);
}

void TemporalScenarioPresenter::setHeight(int height)
{
    m_view->setHeight(height);
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

}

void TemporalScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(auto constraint : m_constraints)
    {
        constraint->on_zoomRatioChanged(m_zoomRatio);
        m_viewInterface->on_constraintMoved(constraint->abstractConstraintViewModel()->model()->id());
    }

    for(auto event : m_events)
    {
        m_viewInterface->on_eventMoved(event->id());
    }
}

void TemporalScenarioPresenter::on_eventCreated(id_type<EventModel> eventId)
{
    on_eventCreated_impl(model(m_viewModel)->event(eventId));
}

void TemporalScenarioPresenter::on_timeNodeCreated(id_type<TimeNodeModel> timeNodeId)
{
    on_timeNodeCreated_impl(model(m_viewModel)->timeNode(timeNodeId));
}

void TemporalScenarioPresenter::on_constraintCreated(id_type<AbstractConstraintViewModel> constraintViewModelId)
{
    on_constraintCreated_impl(constraintViewModel(m_viewModel, constraintViewModelId));
}

void TemporalScenarioPresenter::on_eventDeleted(id_type<EventModel> eventId)
{
    removeFromVectorWithId(m_events, eventId);
    m_view->update();
}

void TemporalScenarioPresenter::on_timeNodeDeleted(id_type<TimeNodeModel> timeNodeId)
{
    removeFromVectorWithId(m_timeNodes, timeNodeId);
    m_view->update();
}

void TemporalScenarioPresenter::on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintViewModelId)
{
    vec_erase_remove_if(m_constraints,
                        [&constraintViewModelId](TemporalConstraintPresenter * pres)
    {
        bool to_delete = viewModel(pres)->id() == constraintViewModelId;

        if(to_delete)
        {
            delete pres;
        }

        return to_delete;
    });

    m_view->update();
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void TemporalScenarioPresenter::on_askUpdate()
{
    m_view->update();
}

void TemporalScenarioPresenter::addTimeNodeToEvent(id_type<EventModel> eventId,
                                                   id_type<TimeNodeModel> timeNodeId)
{
    // TODO why does the model changes here ?????????
    // TODO make the models const* to prevent this...
    auto event = findById(m_events, eventId);
    event->model()->changeTimeNode(timeNodeId);
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void TemporalScenarioPresenter::on_eventCreated_impl(EventModel* event_model)
{
    auto rect = m_view->boundingRect();

    auto event_presenter = new EventPresenter {event_model,
                           m_view,
                           this};

    event_presenter
            ->view()
            ->setPos({rect.x() + event_model->date().toPixels(m_zoomRatio),
                      rect.y() + rect.height() * event_model->heightPercentage() });

    connect(event_presenter,    &EventPresenter::eventHoverEnter,
            [=] ()
    {   m_viewInterface->on_hoverOnEvent(event_presenter->id(), true); });

    connect(event_presenter,    &EventPresenter::eventHoverLeave,
            [=] ()
    {   m_viewInterface->on_hoverOnEvent(event_presenter->id(), false); });

    m_events.push_back(event_presenter);
}

void TemporalScenarioPresenter::on_timeNodeCreated_impl(TimeNodeModel* timeNode_model)
{
    auto rect = m_view->boundingRect();

    auto timeNode_presenter = new TimeNodePresenter {
            timeNode_model,
            m_view,
            this};

    timeNode_presenter
            ->view()
            ->setPos({timeNode_model->date().toPixels(m_zoomRatio),
                      timeNode_model->y() * rect.height()});

    m_timeNodes.push_back(timeNode_presenter);
    m_viewInterface->updateTimeNode(timeNode_model->id());

    connect(timeNode_presenter, &TimeNodePresenter::eventAdded,
            this,               &TemporalScenarioPresenter::addTimeNodeToEvent);
}

void TemporalScenarioPresenter::on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model)
{
    auto rect = m_view->boundingRect();

    auto constraint_presenter = new TemporalConstraintPresenter{
                                constraint_view_model,
                                m_view,
                                this};

    constraint_presenter->view()->setPos({rect.x() + constraint_view_model->model()->startDate().toPixels(m_zoomRatio),
                             rect.y() + rect.height() * constraint_view_model->model()->heightPercentage() });

    constraint_presenter->on_zoomRatioChanged(m_zoomRatio);

    m_constraints.push_back(constraint_presenter);


    connect(constraint_presenter,	&TemporalConstraintPresenter::askUpdate,
            this,					&TemporalScenarioPresenter::on_askUpdate);

    connect(constraint_presenter,   &TemporalConstraintPresenter::constraintHoverEnter,
            [=] ()
    {
        m_viewInterface->on_hoverOnConstraint(constraint_presenter->model()->id(), true);
    });
    connect(constraint_presenter,   &TemporalConstraintPresenter::constraintHoverLeave,
            [=] ()
    {
        m_viewInterface->on_hoverOnConstraint(constraint_presenter->model()->id(), false);
    });

    // TODO Select constraint here.
    m_viewInterface->updateTimeNode(findById(m_events,
                                             constraint_view_model->model()->endEvent())
                                        ->model()->timeNode());
    m_viewInterface->updateTimeNode(findById(m_events,
                                             constraint_view_model->model()->startEvent())
                                        ->model()->timeNode());
}

void TemporalScenarioPresenter::focus()
{
    m_focusDispatcher.focus(m_viewModel);
}

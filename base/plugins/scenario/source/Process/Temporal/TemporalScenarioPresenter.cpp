#include "TemporalScenarioPresenter.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventData.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "ProcessInterface/ZoomHelper.hpp"

#include "ScenarioCommandManager.hpp"
#include "ScenarioViewInterface.hpp"

#include <core/interface/selection/Selection.hpp>
#include <tools/utilsCPP11.hpp>

#include <QDebug>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QStateMachine>
// TODO use state machine for selection (ctrl / not ctrl)
#include <QApplication>
#include <QtMath>
#include "core/presenter/command/SerializableCommand.hpp"

TemporalScenarioPresenter::TemporalScenarioPresenter(ProcessViewModelInterface* process_view_model,
                                                     ProcessViewInterface* view,
                                                     QObject* parent) :
    ProcessPresenterInterface {"TemporalScenarioPresenter", parent},
    m_viewModel {static_cast<TemporalScenarioViewModel*>(process_view_model) },
    m_view {static_cast<TemporalScenarioView*>(view) }
{
    m_cmdManager = new ScenarioCommandManager{this};
    m_viewInterface = new ScenarioViewInterface{this};

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
    // TODO make it more generic (maybe with a QAction ?)
    connect(m_view, &TemporalScenarioView::deletePressed,
            [ = ]()
    {
        m_cmdManager->deleteSelection();
    });
    connect(m_view, &TemporalScenarioView::clearPressed,
            [ = ]()
    {
        m_cmdManager->clearContentFromSelection();
    });

    connect(m_view, &TemporalScenarioView::scenarioPressed,
            this,	&TemporalScenarioPresenter::on_scenarioPressed);

    connect(m_view, &TemporalScenarioView::scenarioReleased,
            [ = ](QPointF point, QPointF scenePoint)
    {
        m_cmdManager->on_scenarioReleased(point, scenePoint);
    });

    connect(m_view, &TemporalScenarioView::newSelectionArea,
            [ = ](QRectF area)
    {
        m_viewInterface->setSelectionArea(area);
    });

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

    connect(m_viewModel, &TemporalScenarioViewModel::eventMoved,
            [ = ](id_type<EventModel> eventId)
    {
        m_viewInterface->on_eventMoved(eventId);
    });
    connect(m_viewModel, &TemporalScenarioViewModel::constraintMoved,
            [ = ](id_type<ConstraintModel> constraintId)
    {
        m_viewInterface->on_constraintMoved(constraintId);
    });

    connect(model(m_viewModel), &ScenarioModel::locked,
            [&]()
    {
        m_view->lock();
    });
    connect(model(m_viewModel), &ScenarioModel::unlocked,
            [&]()
    {
        m_view->unlock();
    });

}

TemporalScenarioPresenter::~TemporalScenarioPresenter()
{
    delete m_cmdManager;
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

// TODO : mettre dans ScenarioViewInterface ?
void TemporalScenarioPresenter::on_horizontalZoomChanged(int val)
{
    m_horizontalZoomSliderVal = val;
    m_millisecPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);

    for(auto constraint : m_constraints)
    {
        constraint->on_horizontalZoomChanged(val);
        m_viewInterface->on_constraintMoved(constraint->abstractConstraintViewModel()->model()->id());
    }

    for(auto event : m_events)
    {
        m_viewInterface->on_eventMoved(event->id());
    }
}

long TemporalScenarioPresenter::millisecPerPixel() const
{
    return m_millisecPerPixel;
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

void TemporalScenarioPresenter::on_scenarioPressed()
{
    // TODO Redo this correctly
    /*
    for(auto& event : m_events)
    {
        event->deselect();
    }

    for(auto& constraint : m_constraints)
    {
        constraint->deselect();
    }

    for(auto& timeNode : m_timeNodes)
    {
        timeNode->deselect();
    }*/
}

void TemporalScenarioPresenter::on_askUpdate()
{
    m_view->update();
}

template<typename InputVector, typename OutputVector>
void copyIfSelected(const InputVector& in, OutputVector& out)
{
    std::copy_if(begin(in),
                 end(in),
                 back_inserter(out),
                 [](typename InputVector::value_type c)
    {
        return c->isSelected();
    });
}

void TemporalScenarioPresenter::addTimeNodeToEvent(id_type<EventModel> eventId,
                                                   id_type<TimeNodeModel> timeNodeId)
{
    auto event = findById(m_events, eventId);
    event->model()->changeTimeNode(timeNodeId);
}

bool TemporalScenarioPresenter::ongoingCommand()
{
    return m_cmdManager->ongoingCommand();
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
template<typename T>
Selection filterSelections(T* pressedModel, Selection sel)
{
    auto cumulation = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
    if(!cumulation)
    {
        sel.clear();
    }

    // If the pressed element is selected
    if(pressedModel->selection.get())
    {
        if(cumulation)
        {
            sel.removeAll(pressedModel);
        }
        else
        {
            sel.push_back(pressedModel);
        }
    }
    else
    {
        sel.push_back(pressedModel);
    }

    return sel;
}

void TemporalScenarioPresenter::on_eventCreated_impl(EventModel* event_model)
{
    auto rect = m_view->boundingRect();

    auto event_view = new EventView {m_view};
    auto event_presenter = new EventPresenter {event_model,
                           event_view,
                           this};
    event_view->setPos({qreal(rect.x() + event_model->date().msec() / m_millisecPerPixel),
                        rect.y() + rect.height() * event_model->heightPercentage() });

    m_events.push_back(event_presenter);

    m_cmdManager->setupEventPresenter(event_presenter);

    // ------------------------------------------------------------
    // Selection
    connect(event_presenter, &EventPresenter::eventPressed,
            [=]()
    {
        auto scenar  = static_cast<ScenarioModel*>(this->m_viewModel->sharedProcessModel());
        emit newSelection(filterSelections(event_presenter->model(),
                                           scenar->selectedChildren()));
    });
}

void TemporalScenarioPresenter::on_timeNodeCreated_impl(TimeNodeModel* timeNode_model)
{
    auto rect = m_view->boundingRect();

    auto timeNode_view = new TimeNodeView {m_view};
    auto timeNode_presenter = new TimeNodePresenter {timeNode_model,
                              timeNode_view,
                              this
};

    timeNode_view->setPos({ (qreal)(timeNode_model->date().msec() / m_millisecPerPixel),
                            timeNode_model->y() * rect.height()
                          });

    m_timeNodes.push_back(timeNode_presenter);
    m_viewInterface->updateTimeNode(timeNode_model->id());

    connect(timeNode_presenter, &TimeNodePresenter::eventAdded,
            this,               &TemporalScenarioPresenter::addTimeNodeToEvent);

    m_cmdManager->setupTimeNodePresenter(timeNode_presenter);

    // ------------------------------------------------------------
    // Selection
    connect(timeNode_presenter, &TimeNodePresenter::timeNodePressed,
            [=]()
    {
        auto scenar  = static_cast<ScenarioModel*>(this->m_viewModel->sharedProcessModel());
        emit newSelection(filterSelections(timeNode_presenter->model(),
                                           scenar->selectedChildren()));
    });
}

void TemporalScenarioPresenter::on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model)
{
    auto rect = m_view->boundingRect();

    auto constraint_view = new TemporalConstraintView {m_view};
    auto constraint_presenter = new TemporalConstraintPresenter{
                                constraint_view_model,
                                constraint_view,
                                this};

    constraint_view->setPos({qreal(rect.x() + constraint_view_model->model()->startDate().msec() / m_millisecPerPixel),
                             rect.y() + rect.height() * constraint_view_model->model()->heightPercentage() });

    constraint_presenter->on_horizontalZoomChanged(m_horizontalZoomSliderVal);

    m_constraints.push_back(constraint_presenter);

    connect(constraint_presenter,	&TemporalConstraintPresenter::submitCommand,
            this,					&TemporalScenarioPresenter::submitCommand);


    connect(constraint_presenter,	&TemporalConstraintPresenter::askUpdate,
            this,					&TemporalScenarioPresenter::on_askUpdate);

    m_cmdManager->setupConstraintPresenter(constraint_presenter);

    // ------------------------------------------------------------
    // selection and inspector management
    connect(constraint_presenter, &TemporalConstraintPresenter::constraintPressed,
            [=]()
    {
        auto scenar  = static_cast<ScenarioModel*>(this->m_viewModel->sharedProcessModel());
        emit newSelection(filterSelections(viewModel(constraint_presenter)->model(),
                                           scenar->selectedChildren()));
    });

    // TODO this could be Abstract'ed...
    connect(constraint_presenter, &TemporalConstraintPresenter::newSelection,
            this, &TemporalScenarioPresenter::newSelection);

    m_viewInterface->updateTimeNode(findById(m_events, constraint_view_model->model()->endEvent())->model()->timeNode());
    m_viewInterface->updateTimeNode(findById(m_events, constraint_view_model->model()->startEvent())->model()->timeNode());
}

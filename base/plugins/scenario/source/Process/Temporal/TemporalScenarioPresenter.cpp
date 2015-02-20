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

#include "Commands/Scenario/CreateEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/CreateConstraint.hpp"
#include "Commands/Scenario/RemoveConstraint.hpp"
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/RemoveMultipleElements.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>

using namespace Scenario;
using namespace Command;
TemporalScenarioPresenter::TemporalScenarioPresenter(ProcessViewModelInterface* process_view_model,
												   ProcessViewInterface* view,
												   QObject* parent):
	ProcessPresenterInterface{"TemporalScenarioPresenter", parent},
	m_viewModel{static_cast<TemporalScenarioViewModel*>(process_view_model)},
	m_view{static_cast<TemporalScenarioView*>(view)}
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
	connect(this,	SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));
    connect(this,	SIGNAL(lastElementSelected()),
            parent, SIGNAL(lastElementSelected()));

	connect(m_view, &TemporalScenarioView::deletePressed,
			this,	&TemporalScenarioPresenter::on_deletePressed);
    connect(m_view, &TemporalScenarioView::clearPressed,
            this,	&TemporalScenarioPresenter::on_clearPressed);
    connect(m_view, &TemporalScenarioView::scenarioPressed,
			this,	&TemporalScenarioPresenter::on_scenarioPressed);
	connect(m_view, &TemporalScenarioView::scenarioPressedWithControl,
			this,	&TemporalScenarioPresenter::on_scenarioPressedWithControl);
	connect(m_view, &TemporalScenarioView::scenarioReleased,
			this,	&TemporalScenarioPresenter::on_scenarioReleased);

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
			this,		 &TemporalScenarioPresenter::on_eventMoved);
	connect(m_viewModel, &TemporalScenarioViewModel::constraintMoved,
			this,		 &TemporalScenarioPresenter::on_constraintMoved);

	connect(model(m_viewModel), &ScenarioModel::locked,
			[&] () { m_view->lock(); });
	connect(model(m_viewModel), &ScenarioModel::unlocked,
			[&] () { m_view->unlock(); });

}

TemporalScenarioPresenter::~TemporalScenarioPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
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

void TemporalScenarioPresenter::putBack()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
	m_view->setOpacity(0.1);
}

void TemporalScenarioPresenter::parentGeometryChanged()
{

}

void TemporalScenarioPresenter::on_horizontalZoomChanged(int val)
{
	m_horizontalZoomSliderVal = val;
	m_millisecPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);

	for(auto constraint : m_constraints)
	{
		constraint->on_horizontalZoomChanged(val);
        on_constraintMoved(constraint->abstractConstraintViewModel()->model()->id());
	}

	for(auto event : m_events)
	{
		on_eventMoved(event->id());
	}
}

id_type<EventModel> TemporalScenarioPresenter::currentlySelectedEvent() const
{
	return m_currentlySelectedEvent;
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
						[&constraintViewModelId] (TemporalConstraintPresenter* pres)
						{
							bool to_delete = viewModel(pres)->id() == constraintViewModelId;
							if(to_delete) delete pres;
							return to_delete;
						} );

	m_view->update();
}

/////////////////////////////////////////////////////////////////////
// MOVING ELEMENTS

void TemporalScenarioPresenter::on_eventMoved(id_type<EventModel> eventId)
{
	auto rect = m_view->boundingRect();
	auto ev = findById(m_events, eventId);

	ev->view()->setPos({qreal(ev->model()->date().msec() / m_millisecPerPixel),
						rect.height() * ev->model()->heightPercentage()});

    if (m_ongoingCommand)
        ev->view()->setMoving(true);
    else
        ev->view()->setMoving(false);

	// @todo change when multiple event on a same timeNode
//	qDebug() << ev->model()->timeNode();
	auto timeNode = findById(m_timeNodes, ev->model()->timeNode());
	timeNode->view()->setPos({qreal(timeNode->model()->date().msec() / m_millisecPerPixel),
							  rect.height() * timeNode->model()->y()});

    updateTimeNode(timeNode->id());
	m_view->update();
}


void TemporalScenarioPresenter::on_constraintMoved(id_type<ConstraintModel> constraintId)
{
	auto rect = m_view->boundingRect();

	for(TemporalConstraintPresenter* pres : m_constraints)
	{
		ConstraintModel* cstr_model{viewModel(pres)->model()};
		if(cstr_model->id() == constraintId )
		{
            auto delta = (view(pres)->x() - (qreal(cstr_model->startDate().msec()) / m_millisecPerPixel));
            bool dateChanged = ( delta * delta > 1 );
            if (dateChanged)
                view(pres)->setPos({qreal(cstr_model->startDate().msec()) / m_millisecPerPixel,
                                           rect.height() * cstr_model->heightPercentage()});

			view(pres)->setDefaultWidth(cstr_model->defaultDuration().msec() / m_millisecPerPixel);
			view(pres)->setMinWidth(cstr_model->minDuration().msec() / m_millisecPerPixel);
			view(pres)->setMaxWidth(cstr_model->maxDuration().msec() / m_millisecPerPixel);

            if (m_ongoingCommand)
                view(pres)->setMoving(true);
            else
                view(pres)->setMoving(false);

            auto endTimeNode = findById(m_events, cstr_model->endEvent())->model()->timeNode();
            updateTimeNode(endTimeNode);

            if (cstr_model->startDate().msec() != 0 && dateChanged )
            {
                auto startTimeNode = findById(m_events, cstr_model->startEvent())->model()->timeNode();
                updateTimeNode(startTimeNode);
			}
		}
	}
	m_view->update();
}

void TemporalScenarioPresenter::updateTimeNode(id_type<TimeNodeModel> id)
{
    auto timeNode = findById(m_timeNodes, id);
    auto rect = m_view->boundingRect();

    double min = 1.0;
    double max = 0.0;

    for (auto eventId : timeNode->model()->events() )
    {
        auto event = findById(m_events, eventId);
        double y = event->model()->heightPercentage();

        if (y < min)
            min = y;
        if (y > max)
            max = y;

        for(TemporalConstraintPresenter* cstr_pres : m_constraints)
        {
			ConstraintModel* cstr_model{cstr_pres->abstractConstraintViewModel()->model()};
            for (auto cstrId : event->model()->previousConstraints() )
            {
                if(cstr_model->id() == cstrId )
                {
                    y = cstr_model->heightPercentage();
                    if (y < min)
                        min = y;
                    if (y > max)
                        max = y;
                }
            }
            for (auto cstrId : event->model()->nextConstraints() )
            {
                if(cstr_model->id() == cstrId )
                {
                    y = cstr_model->heightPercentage();
                    if (y < min)
                        min = y;
                    if (y > max)
                        max = y;
                }
            }
        }

    }
    min -= timeNode->model()->y();
    max -= timeNode->model()->y();

	timeNode->view()->setExtremities(int(rect.height() * min), int(rect.height() * max));
    if (m_ongoingCommand)
        timeNode->view()->setMoving(true);
    else
        timeNode->view()->setMoving(false);

}


#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
void TemporalScenarioPresenter::sendOngoingCommand(iscore::SerializableCommand* cmd)
{
	if(m_ongoingCommand && cmd->id() != m_ongoingCommandId)
	{
		rollbackOngoingCommand();
	}

	auto doc = iscore::documentFromObject(m_viewModel->sharedProcessModel());
	if(!m_ongoingCommand)
	{
		m_ongoingCommand = true;
		m_ongoingCommandId = cmd->id();
		doc->presenter()->initiateOngoingCommand(cmd, m_viewModel->sharedProcessModel());
	}
	else
	{
		doc->presenter()->continueOngoingCommand(cmd);
	}
}

void TemporalScenarioPresenter::finishOngoingCommand()
{
	auto doc = iscore::documentFromObject(m_viewModel->sharedProcessModel());
    m_ongoingCommand = false;
    doc->presenter()->validateOngoingCommand();
    m_ongoingCommandId = -1;
}

void TemporalScenarioPresenter::rollbackOngoingCommand()
{
	auto doc = iscore::documentFromObject(m_viewModel->sharedProcessModel());
	doc->presenter()->rollbackOngoingCommand();
	m_ongoingCommand = false;
	m_ongoingCommandId = -1;
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS

void TemporalScenarioPresenter::on_deletePressed()
{
    deleteSelection();
}

void TemporalScenarioPresenter::on_clearPressed()
{
    clearContentFromSelection();
}

void TemporalScenarioPresenter::on_scenarioPressed()
{
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
    }
}


void TemporalScenarioPresenter::on_scenarioPressedWithControl(QPointF point, QPointF scenePoint)
{

}

void TemporalScenarioPresenter::on_scenarioReleased(QPointF point, QPointF scenePoint)
{
    EventData data{};
    data.eventClickedId = m_events.back()->id();
    data.x = point.x();
	data.dDate.setMSecs(point.x() * m_millisecPerPixel);
    data.y = point.y();
    data.relativeY = point.y() /  m_view->boundingRect().height();
    data.scenePos = scenePoint;

    TimeNodeView* tnv =  dynamic_cast<TimeNodeView*>(this->m_view->scene()->itemAt(scenePoint, QTransform()));
    if (tnv)
    {
        for (auto timeNode : m_timeNodes)
        {
            if (timeNode->view() == tnv)
            {
                data.endTimeNodeId = timeNode->id();
                data.dDate = timeNode->model()->date();
				data.x = data.dDate.msec() / m_millisecPerPixel;
				break;
            }
        }
    }

	auto cmd = new CreateEvent(ObjectPath::pathFromObject("BaseElementModel",
														  m_viewModel->sharedProcessModel()),
							   data);
    this->submitCommand(cmd);

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
				 [] (typename InputVector::value_type c) { return c->isSelected(); });
}

void TemporalScenarioPresenter::clearContentFromSelection()
{
	using namespace Scenario::Command;
	// 1. Select items
	std::vector<TemporalConstraintPresenter*> constraintsToRemove;
	std::vector<EventPresenter*> eventsToRemove;

	copyIfSelected(m_constraints, constraintsToRemove);
	copyIfSelected(m_events, eventsToRemove);

	QVector<iscore::SerializableCommand*> commands;

	// 3. Create a Delete command for each. For now : only emptying.
    for(auto& constraint : constraintsToRemove)
	{
		commands.push_back(
					new ClearConstraint(
						ObjectPath::pathFromObject("BaseElementModel",
												   viewModel(constraint)->model())));
	}

    for(auto& event : eventsToRemove)
	{
		commands.push_back(
					new ClearEvent(
						ObjectPath::pathFromObject("BaseElementModel",
												   event->model())));
	}

	// 4. Make a meta-command that binds them all and calls undo & redo on the queue.
	auto cmd = new RemoveMultipleElements{std::move(commands)};
    emit submitCommand(cmd);
}

void TemporalScenarioPresenter::deleteSelection()
{
    // TODO quelques comportements bizarres à régler ...

   //*
    using namespace Scenario::Command;
    // 1. Select items
    std::vector<TemporalConstraintPresenter*> constraintsToRemove;
    std::vector<EventPresenter*> eventsToRemove;

    copyIfSelected(m_constraints, constraintsToRemove);
    copyIfSelected(m_events, eventsToRemove);

    if (constraintsToRemove.size() != 0 || eventsToRemove.size() != 0 )
    {
        QVector<iscore::SerializableCommand*> commands;

        // 2. Create a Delete command for each. For now : only emptying.
        for(auto& constraint : constraintsToRemove)
        {
            commands.push_back(
                        new RemoveConstraint(
                            ObjectPath::pathFromObject("BaseElementModel",
                                                       m_viewModel->sharedProcessModel()),
                            constraint->abstractConstraintViewModel()->model() ));
        }

        for(auto& event : eventsToRemove)
        {
            commands.push_back(
                        new RemoveEvent(
                            ObjectPath::pathFromObject("BaseElementModel",
                                                       m_viewModel->sharedProcessModel()),
                            event->model()) );

        }

        // 3. Make a meta-command that binds them all and calls undo & redo on the queue.
        auto cmd = new RemoveMultipleElements{std::move(commands)};

        if (cmd) emit submitCommand(cmd);
    }
   // */
}

void TemporalScenarioPresenter::setCurrentlySelectedEvent(id_type<EventModel> arg)
{
	if (m_currentlySelectedEvent != arg)
	{
		m_currentlySelectedEvent = arg;
		emit currentlySelectedEventChanged(arg);
	}
}

void TemporalScenarioPresenter::addTimeNodeToEvent(id_type<EventModel> eventId, id_type<TimeNodeModel> timeNodeId)
{
    auto event = findById(m_events, eventId);
    event->model()->changeTimeNode(timeNodeId);
}

#include <algorithm>

// Three cases :
// We are between :
//  an event and nothing -> CreateEventAfterEvent
//  an event and a timenode -> CreateEventAfterEventOnTimeNode
//  an event and another event -> CreateConstraint
void TemporalScenarioPresenter::createConstraint(EventData data)
{
	using namespace std;
	data.dDate.setMSecs(data.x * m_millisecPerPixel - model(m_viewModel)->event(data.eventClickedId)->date().msec());
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmdPath = ObjectPath::pathFromObject("BaseElementModel",
											  m_viewModel->sharedProcessModel());

	// We rollback so that we don't get polluted
	// by the "fake" created events / timenodes.
	if(m_ongoingCommand)
		rollbackOngoingCommand();

	QList<EventPresenter*> collidingEvents;
	copy_if(begin(m_events), end(m_events), back_inserter(collidingEvents),
			[] (EventPresenter* ev) { return ev->view()->isUnderMouse(); });

	QList<TimeNodePresenter*> collidingTimeNodes;
	copy_if(begin(m_timeNodes), end(m_timeNodes), back_inserter(collidingTimeNodes),
			[] (TimeNodePresenter* tn) { return tn->view()->isUnderMouse(); });

	if(collidingEvents.empty())
	{
		if(collidingTimeNodes.empty())
		{
			sendOngoingCommand(new CreateEventAfterEvent(move(cmdPath), data));
		}
		else
		{
			auto tn = collidingTimeNodes.first();
			data.endTimeNodeId = tn->id();
			data.dDate = tn->model()->date() - model(m_viewModel)->event(data.eventClickedId)->date();

			sendOngoingCommand(new CreateEventAfterEventOnTimeNode(move(cmdPath), data));
		}
	}
	else
	{
		sendOngoingCommand(new CreateConstraint(move(cmdPath),
								   data.eventClickedId,
								   collidingEvents.first()->id()));
    }
}


/////////////////////////////////////////////////////////////////////
// MOVING ELEMENTS COMMANDS

void TemporalScenarioPresenter::moveEventAndConstraint(EventData data)
{
	data.dDate.setMSecs(data.x * m_millisecPerPixel);
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmd = new Command::MoveEvent(ObjectPath::pathFromObject("BaseElementModel",
															   m_viewModel->sharedProcessModel()),
									data);

	sendOngoingCommand(cmd);
}

void TemporalScenarioPresenter::moveConstraint(ConstraintData data)
{
	data.dDate.setMSecs(data.x * m_millisecPerPixel);
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmd = new Command::MoveConstraint(ObjectPath::pathFromObject("BaseElementModel",
																	m_viewModel->sharedProcessModel()),
									data);


	sendOngoingCommand(cmd);
}

void TemporalScenarioPresenter::moveTimeNode(EventData data)
{
    auto ev = findById(m_events, data.eventClickedId);
    data.y = ev->view()->y();
	moveEventAndConstraint(data);
}

void TemporalScenarioPresenter::on_ctrlStateChanged(bool ctrlPressed)
{
	if(!m_ongoingCommand)
		return;

	rollbackOngoingCommand();

	if(ctrlPressed)
	{ createConstraint(m_lastData); }
	else
	{ moveEventAndConstraint(m_lastData); }
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED

void TemporalScenarioPresenter::on_eventCreated_impl(EventModel* event_model)
{
	auto rect = m_view->boundingRect();

	auto event_view = new EventView{m_view};
	auto event_presenter = new EventPresenter{event_model,
											  event_view,
											  this};
    event_view->setPos({qreal(rect.x() + event_model->date().msec() / m_millisecPerPixel),
						rect.y() + rect.height() * event_model->heightPercentage()});

	m_events.push_back(event_presenter);
	connect(event_presenter, &EventPresenter::eventSelected,
			this,			 &TemporalScenarioPresenter::setCurrentlySelectedEvent);

	connect(event_presenter, &EventPresenter::eventMoved,
			this,			 &TemporalScenarioPresenter::moveEventAndConstraint);
	connect(event_presenter, &EventPresenter::eventMovedWithControl,
			this,			 &TemporalScenarioPresenter::createConstraint);
	connect(event_presenter, &EventPresenter::eventReleased,
			this,			 &TemporalScenarioPresenter::finishOngoingCommand);
	connect(event_presenter, &EventPresenter::eventReleasedWithControl,
			this,			 &TemporalScenarioPresenter::finishOngoingCommand);

	connect(event_presenter, &EventPresenter::ctrlStateChanged,
			this,			 &TemporalScenarioPresenter::on_ctrlStateChanged);

	connect(event_presenter, &EventPresenter::elementSelected,
			this,			 &TemporalScenarioPresenter::elementSelected);

    connect(event_presenter,    &EventPresenter::constraintSelected,
            [=] (QString cstrId)
    {
        for (TemporalConstraintPresenter* cstr : m_constraints)
        {
            if (*viewModel(cstr)->model()->id().val() == cstrId.toInt())
            {
                elementSelected( viewModel(cstr));
                event_presenter->deselect();
                view(cstr)->setSelected(true);
                return;
            }
        }
        m_lastInspectedObjects.push_back(event_model);
    });

    connect(event_presenter,    &EventPresenter::inspectPreviousElement,
            this,               &TemporalScenarioPresenter::lastElementSelected);
}

void TemporalScenarioPresenter::on_timeNodeCreated_impl(TimeNodeModel* timeNode_model)
{
	auto rect = m_view->boundingRect();

	auto timeNode_view = new TimeNodeView{m_view};
	auto timeNode_presenter = new TimeNodePresenter{timeNode_model,
													timeNode_view,
													this};

	timeNode_view->setPos({(qreal) (timeNode_model->date().msec() / m_millisecPerPixel),
						   timeNode_model->y() * rect.height()});

    m_timeNodes.push_back(timeNode_presenter);
    updateTimeNode(timeNode_model->id());

	connect(timeNode_presenter, &TimeNodePresenter::timeNodeMoved,
			this,				&TemporalScenarioPresenter::moveTimeNode);
    connect(timeNode_presenter, &TimeNodePresenter::timeNodeReleased,
			this,				&TemporalScenarioPresenter::finishOngoingCommand);

    connect(timeNode_presenter, &TimeNodePresenter::elementSelected,
			this,				&TemporalScenarioPresenter::elementSelected);

    connect(timeNode_presenter, &TimeNodePresenter::eventAdded,
            this,               &TemporalScenarioPresenter::addTimeNodeToEvent);

    connect(timeNode_presenter, &TimeNodePresenter::eventSelected,
            [=] (QString evId)
    {
        auto event = findById(m_events, id_type<EventModel>(evId.toInt()));
        event->view()->setSelected(true);
        elementSelected(event->model());
        timeNode_presenter->deselect();
    });

    connect(timeNode_presenter, &TimeNodePresenter::inspectPreviousElement,
            this,               &TemporalScenarioPresenter::lastElementSelected);
}

void TemporalScenarioPresenter::on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model)
{
	auto rect = m_view->boundingRect();

	auto constraint_view = new TemporalConstraintView{m_view};
	auto constraint_presenter = new TemporalConstraintPresenter{
													constraint_view_model,
													constraint_view,
													this};

    constraint_view->setPos({qreal(rect.x() + constraint_view_model->model()->startDate().msec() / m_millisecPerPixel),
                             rect.y() + rect.height() * constraint_view_model->model()->heightPercentage()});

	constraint_presenter->on_horizontalZoomChanged(m_horizontalZoomSliderVal);

	m_constraints.push_back(constraint_presenter);

	connect(constraint_presenter,	&TemporalConstraintPresenter::constraintMoved,
			this,					&TemporalScenarioPresenter::moveConstraint);
	connect(constraint_presenter,	&TemporalConstraintPresenter::constraintReleased,
			this,					&TemporalScenarioPresenter::finishOngoingCommand);

	connect(constraint_presenter,	&TemporalConstraintPresenter::submitCommand,
			this,					&TemporalScenarioPresenter::submitCommand);
	connect(constraint_presenter,	&TemporalConstraintPresenter::elementSelected,
			this,					&TemporalScenarioPresenter::elementSelected);

    connect(constraint_presenter,	&TemporalConstraintPresenter::askUpdate,
            this,					&TemporalScenarioPresenter::on_askUpdate);

    connect(constraint_presenter,   &TemporalConstraintPresenter::eventSelected,
            [=] (QString evId)
    {
        auto event = findById(m_events, id_type<EventModel>(evId.toInt()));
        event->view()->setSelected(true);
        m_lastInspectedObjects.push_back(constraint_view_model);
        elementSelected(event->model());
        constraint_presenter->deselect();
    });

    updateTimeNode( findById(m_events, constraint_view_model->model()->endEvent())->model()->timeNode());
    updateTimeNode( findById(m_events, constraint_view_model->model()->startEvent())->model()->timeNode());
}

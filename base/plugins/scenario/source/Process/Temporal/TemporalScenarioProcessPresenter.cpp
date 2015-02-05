#include "TemporalScenarioProcessPresenter.hpp"

#include "source/Process/ScenarioProcessSharedModel.hpp"
#include "source/Process/Temporal/TemporalScenarioProcessViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioProcessView.hpp"

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
#include "Commands/Scenario/CreateConstraint.hpp"
#include "Commands/Scenario/RemoveConstraint.hpp"
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/RemoveMultipleElements.hpp"
#include "Document/ZoomHelper.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>

using namespace Scenario;
TemporalScenarioProcessPresenter::TemporalScenarioProcessPresenter(ProcessViewModelInterface* process_view_model,
												   ProcessViewInterface* view,
												   QObject* parent):
	ProcessPresenterInterface{"TemporalScenarioProcessPresenter", parent},
	m_viewModel{static_cast<TemporalScenarioProcessViewModel*>(process_view_model)},
	m_view{static_cast<TemporalScenarioProcessView*>(view)}
{
	/////// Setup of existing data
	// For each constraint & event, display' em
	for(auto constraint_view_model : constraintsViewModels(m_viewModel))
	{
		on_constraintCreated_impl(constraint_view_model);
	}

	for(auto event_model : model(m_viewModel)->events())
	{
		on_eventCreated_impl(event_model);
	}

	for(auto tn_model : model(m_viewModel)->timeNodes())
	{
		on_timeNodeCreated_impl(tn_model);
	}
	/////// Connections
	connect(this,	SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));

	connect(m_view, &TemporalScenarioProcessView::deletePressed,
			this,	&TemporalScenarioProcessPresenter::on_deletePressed);
    connect(m_view, &TemporalScenarioProcessView::clearPressed,
            this,	&TemporalScenarioProcessPresenter::on_clearPressed);
    connect(m_view, &TemporalScenarioProcessView::scenarioPressed,
			this,	&TemporalScenarioProcessPresenter::on_scenarioPressed);
	connect(m_view, &TemporalScenarioProcessView::scenarioPressedWithControl,
			this,	&TemporalScenarioProcessPresenter::on_scenarioPressedWithControl);
	connect(m_view, &TemporalScenarioProcessView::scenarioReleased,
			this,	&TemporalScenarioProcessPresenter::on_scenarioReleased);

	connect(m_viewModel, &TemporalScenarioProcessViewModel::eventCreated,
			this,		 &TemporalScenarioProcessPresenter::on_eventCreated);
	connect(m_viewModel, &TemporalScenarioProcessViewModel::eventDeleted,
			this,		 &TemporalScenarioProcessPresenter::on_eventDeleted);

	connect(m_viewModel, &TemporalScenarioProcessViewModel::timeNodeCreated,
			this,        &TemporalScenarioProcessPresenter::on_timeNodeCreated);
    connect(m_viewModel, &TemporalScenarioProcessViewModel::timeNodeDeleted,
            this,        &TemporalScenarioProcessPresenter::on_timeNodeDeleted);

	connect(m_viewModel, &TemporalScenarioProcessViewModel::constraintViewModelCreated,
			this,		 &TemporalScenarioProcessPresenter::on_constraintCreated);
	connect(m_viewModel, &TemporalScenarioProcessViewModel::constraintViewModelRemoved,
			this,		 &TemporalScenarioProcessPresenter::on_constraintViewModelRemoved);

	connect(m_viewModel, &TemporalScenarioProcessViewModel::eventMoved,
			this,		 &TemporalScenarioProcessPresenter::on_eventMoved);
	connect(m_viewModel, &TemporalScenarioProcessViewModel::constraintMoved,
			this,		 &TemporalScenarioProcessPresenter::on_constraintMoved);

	connect(model(m_viewModel), &ScenarioProcessSharedModel::locked,
			[&] () { m_view->lock(); });
	connect(model(m_viewModel), &ScenarioProcessSharedModel::unlocked,
			[&] () { m_view->unlock(); });

}

TemporalScenarioProcessPresenter::~TemporalScenarioProcessPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

id_type<ProcessViewModelInterface> TemporalScenarioProcessPresenter::viewModelId() const
{
	return m_viewModel->id();
}

id_type<ProcessSharedModelInterface> TemporalScenarioProcessPresenter::modelId() const
{
	return m_viewModel->sharedProcessModel()->id();
}

void TemporalScenarioProcessPresenter::putToFront()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
	m_view->setOpacity(1);
}

void TemporalScenarioProcessPresenter::putBack()
{
	m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
	m_view->setOpacity(0.1);
}

void TemporalScenarioProcessPresenter::on_horizontalZoomChanged(int val)
{
	m_horizontalZoomSliderVal = val;
	m_millisecPerPixel = secondsPerPixel(m_horizontalZoomSliderVal);

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

id_type<EventModel> TemporalScenarioProcessPresenter::currentlySelectedEvent() const
{
	return m_currentlySelectedEvent;
}

long TemporalScenarioProcessPresenter::millisecPerPixel() const
{
	return m_millisecPerPixel;
}

void TemporalScenarioProcessPresenter::on_eventCreated(id_type<EventModel> eventId)
{
	on_eventCreated_impl(model(m_viewModel)->event(eventId));
}

void TemporalScenarioProcessPresenter::on_timeNodeCreated(id_type<TimeNodeModel> timeNodeId)
{
    on_timeNodeCreated_impl(model(m_viewModel)->timeNode(timeNodeId));
}

void TemporalScenarioProcessPresenter::on_constraintCreated(id_type<AbstractConstraintViewModel> constraintViewModelId)
{
	on_constraintCreated_impl(constraintViewModel(m_viewModel, constraintViewModelId));
}

void TemporalScenarioProcessPresenter::on_eventDeleted(id_type<EventModel> eventId)
{
    removeFromVectorWithId(m_events, eventId);
	m_view->update();
}

void TemporalScenarioProcessPresenter::on_timeNodeDeleted(id_type<TimeNodeModel> timeNodeId)
{
    removeFromVectorWithId(m_timeNodes, timeNodeId);
    m_view->update();
}

void TemporalScenarioProcessPresenter::on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintViewModelId)
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

void TemporalScenarioProcessPresenter::on_eventMoved(id_type<EventModel> eventId)
{
	auto rect = m_view->boundingRect();
	auto ev = findById(m_events, eventId);

	ev->view()->setPos({qreal(ev->model()->date() / m_millisecPerPixel),
						rect.height() * ev->model()->heightPercentage()});

	// @todo change when multiple event on a same timeNode
	auto timeNode = findById(m_timeNodes, ev->model()->timeNode());
	timeNode->view()->setPos({qreal(timeNode->model()->date() / m_millisecPerPixel),
							  rect.height() * timeNode->model()->y()});

    updateTimeNode(timeNode->id());
	m_view->update();
}


void TemporalScenarioProcessPresenter::on_constraintMoved(id_type<ConstraintModel> constraintId)
{
	auto rect = m_view->boundingRect();

	for(TemporalConstraintPresenter* pres : m_constraints)
	{
		ConstraintModel* cstr_model{viewModel(pres)->model()};
		if(cstr_model->id() == constraintId )
		{
			view(pres)->setPos({qreal(cstr_model->startDate()) / m_millisecPerPixel,
									   rect.height() * cstr_model->heightPercentage()});

			view(pres)->setDefaultWidth(cstr_model->defaultDuration() / m_millisecPerPixel);
			view(pres)->setMinWidth(cstr_model->minDuration() / m_millisecPerPixel);
			view(pres)->setMaxWidth(cstr_model->maxDuration() / m_millisecPerPixel);


            auto endTimeNode = findById(m_events, cstr_model->endEvent())->model()->timeNode();
            updateTimeNode(endTimeNode);

            if (cstr_model->startDate() != 0 )
            {
                auto startTimeNode = findById(m_events, cstr_model->startEvent())->model()->timeNode();
                updateTimeNode(startTimeNode);
			}
		}
	}
	m_view->update();
}

void TemporalScenarioProcessPresenter::updateTimeNode(id_type<TimeNodeModel> id)
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
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS

void TemporalScenarioProcessPresenter::on_deletePressed()
{
    deleteSelection();
}

void TemporalScenarioProcessPresenter::on_clearPressed()
{
    clearContentFromSelection();
}

void TemporalScenarioProcessPresenter::on_scenarioPressed()
{
	for(auto& event : m_events)
	{
		event->deselect();
	}
	for(auto& constraint : m_constraints)
	{
		constraint->deselect();
	}
}


void TemporalScenarioProcessPresenter::on_scenarioPressedWithControl(QPointF point, QPointF scenePoint)
{
/*	// @todo maybe better to create event on mouserelease ? And only show a "fake" event + interval on mousepress.
    EventData d;
    d.dDate = point.x() * m_millisecPerPixel;
    d.relativeY = (point - m_view->boundingRect().topLeft() ).y() / m_view->boundingRect().height();
*/
}

void TemporalScenarioProcessPresenter::on_scenarioReleased(QPointF point, QPointF scenePoint)
{
    EventData data{};
    data.eventClickedId = m_events.back()->id();
    data.x = point.x();
    data.dDate = point.x() * m_millisecPerPixel;
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
                data.x = data.dDate / m_millisecPerPixel;
            }
        }
    }

    auto cmd = new Command::CreateEvent(ObjectPath::pathFromObject("BaseConstraintModel",
                                                                 m_viewModel->sharedProcessModel()),
                                        data);
    this->submitCommand(cmd);

}

void TemporalScenarioProcessPresenter::on_askUpdate()
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

void TemporalScenarioProcessPresenter::clearContentFromSelection()
{
	using namespace Scenario::Command;
	// 1. Select items
	std::vector<TemporalConstraintPresenter*> constraintsToRemove;
	std::vector<EventPresenter*> eventsToRemove;

	copyIfSelected(m_constraints, constraintsToRemove);
	copyIfSelected(m_events, eventsToRemove);

	QVector<iscore::SerializableCommand*> commands;

	// 3. Create a Delete command for each. For now : only emptying.
	for(auto& constraint : m_constraints)
	{
		commands.push_back(
					new ClearConstraint(
						ObjectPath::pathFromObject("BaseConstraintModel",
												   viewModel(constraint)->model())));
	}

	for(auto& event : m_events)
	{
		commands.push_back(
					new ClearEvent(
						ObjectPath::pathFromObject("BaseConstraintModel",
												   event->model())));
	}

	// 4. Make a meta-command that binds them all and calls undo & redo on the queue.
	auto cmd = new RemoveMultipleElements{std::move(commands)};
    emit submitCommand(cmd);
}

void TemporalScenarioProcessPresenter::deleteSelection()
{
   //*
    using namespace Scenario::Command;
    // 1. Select items
    std::vector<TemporalConstraintPresenter*> constraintsToRemove;
    std::vector<EventPresenter*> eventsToRemove;

    copyIfSelected(m_constraints, constraintsToRemove);
    copyIfSelected(m_events, eventsToRemove);

    QVector<iscore::SerializableCommand*> commands;

    // 2. Create a Delete command for each. For now : only emptying.
    for(auto& constraint : constraintsToRemove)
    {
        commands.push_back(
                    new RemoveConstraint(
                        ObjectPath::pathFromObject("BaseConstraintModel",
                                                   m_viewModel->sharedProcessModel()),
						constraint->abstractConstraintViewModel()->model() ));
    }

    for(auto& event : eventsToRemove)
    {
        if (! event->model()->nextConstraints().size() )
        {
            commands.push_back(
                        new RemoveEvent(
                            ObjectPath::pathFromObject("BaseConstraintModel",
                                                       m_viewModel->sharedProcessModel()),
                            event->model()) );
        }
    }

    // todo : modifier pour selection multiple

    // 3. Make a meta-command that binds them all and calls undo & redo on the queue.
//    auto cmd = new RemoveMultipleElements{std::move(commands)};

    if (commands.size()) emit submitCommand(commands.at(0));
   // */
}

void TemporalScenarioProcessPresenter::setCurrentlySelectedEvent(id_type<EventModel> arg)
{
	if (m_currentlySelectedEvent != arg)
	{
		m_currentlySelectedEvent = arg;
		emit currentlySelectedEventChanged(arg);
	}
}

void TemporalScenarioProcessPresenter::createConstraint(EventData data)
{
	data.dDate = data.x * m_millisecPerPixel - model(m_viewModel)->event(data.eventClickedId)->date();
	data.relativeY = data.y / m_view->boundingRect().height();

	EventView* it = dynamic_cast<EventView*>(this->m_view->scene()->itemAt(data.scenePos, QTransform()));
	id_type<EventModel> endEvent;

	if (it)
	{
		for (auto& ev : m_events)
		{
			if(ev->view() == it)
			{
				endEvent = ev->id();
				auto cmd = new Command::CreateConstraint(ObjectPath::pathFromObject("BaseConstraintModel",
																					m_viewModel->sharedProcessModel()),
														 data.eventClickedId,
														 endEvent);
				submitCommand(cmd);
				break;
			}
		}
	}
	else
	{
        TimeNodeView* tnv =  dynamic_cast<TimeNodeView*>(this->m_view->scene()->itemAt(data.scenePos, QTransform()));
        if (tnv)
        {
            for (auto timeNode : m_timeNodes)
            {
                if (timeNode->view() == tnv)
                {
                    data.endTimeNodeId = timeNode->id();
                    data.dDate = timeNode->model()->date() - model(m_viewModel)->event(data.eventClickedId)->date();
                }
            }
        }

		auto cmd = new Command::CreateEventAfterEvent(ObjectPath::pathFromObject("BaseConstraintModel",
																			   m_viewModel->sharedProcessModel()),
													data);
		submitCommand(cmd);
	}
}

/////////////////////////////////////////////////////////////////////
// MOVING ELEMENTS COMMANDS

void TemporalScenarioProcessPresenter::moveEventAndConstraint(EventData data)
{
	data.dDate = data.x * m_millisecPerPixel;
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmd = new Command::MoveEvent(ObjectPath::pathFromObject("BaseConstraintModel",
															   m_viewModel->sharedProcessModel()),
									data);
	submitCommand(cmd);
}

void TemporalScenarioProcessPresenter::moveConstraint(ConstraintData data)
{
    data.dDate = data.x * m_millisecPerPixel;
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmd = new Command::MoveConstraint(ObjectPath::pathFromObject("BaseConstraintModel",
																	m_viewModel->sharedProcessModel()),
									data);

    submitCommand(cmd);
}

void TemporalScenarioProcessPresenter::moveTimeNode(EventData data)
{
    auto ev = findById(m_events, data.eventClickedId);
    data.y = ev->view()->y();
    moveEventAndConstraint(data);
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED

void TemporalScenarioProcessPresenter::on_eventCreated_impl(EventModel* event_model)
{
	auto rect = m_view->boundingRect();

	auto event_view = new EventView{m_view};
	auto event_presenter = new EventPresenter{event_model,
											  event_view,
											  this};
	event_view->setPos({rect.x() + event_model->date() / m_millisecPerPixel,
						rect.y() + rect.height() * event_model->heightPercentage()});

	m_events.push_back(event_presenter);
	connect(event_presenter, &EventPresenter::eventSelected,
			this,			 &TemporalScenarioProcessPresenter::setCurrentlySelectedEvent);
	connect(event_presenter, &EventPresenter::eventReleasedWithControl,
			this,			 &TemporalScenarioProcessPresenter::createConstraint);
	connect(event_presenter, &EventPresenter::eventReleased,
			this,			 &TemporalScenarioProcessPresenter::moveEventAndConstraint);
	connect(event_presenter, &EventPresenter::elementSelected,
			this,			 &TemporalScenarioProcessPresenter::elementSelected);
}

void TemporalScenarioProcessPresenter::on_timeNodeCreated_impl(TimeNodeModel* timeNode_model)
{
	auto rect = m_view->boundingRect();

	auto timeNode_view = new TimeNodeView{m_view};
	auto timeNode_presenter = new TimeNodePresenter{timeNode_model,
													timeNode_view,
													this};

	timeNode_view->setPos({(qreal) (timeNode_model->date() / m_millisecPerPixel),
						   timeNode_model->y() * rect.height()});

    m_timeNodes.push_back(timeNode_presenter);
    updateTimeNode(timeNode_model->id());

    connect(timeNode_presenter, &TimeNodePresenter::timeNodeReleased,
            this,			 &TemporalScenarioProcessPresenter::moveTimeNode);

}

void TemporalScenarioProcessPresenter::on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model)
{
	auto rect = m_view->boundingRect();

	auto constraint_view = new TemporalConstraintView{m_view};
	auto constraint_presenter = new TemporalConstraintPresenter{
													constraint_view_model,
													constraint_view,
													this};

    constraint_view->setPos({rect.x() + constraint_view_model->model()->startDate() / m_millisecPerPixel,
							 rect.y() + rect.height() * constraint_view_model->model()->heightPercentage()});

	constraint_presenter->on_horizontalZoomChanged(m_horizontalZoomSliderVal);

	m_constraints.push_back(constraint_presenter);

	connect(constraint_presenter,	&TemporalConstraintPresenter::constraintReleased,
			this,					&TemporalScenarioProcessPresenter::moveConstraint);
	connect(constraint_presenter,	&TemporalConstraintPresenter::submitCommand,
			this,					&TemporalScenarioProcessPresenter::submitCommand);
	connect(constraint_presenter,	&TemporalConstraintPresenter::elementSelected,
			this,					&TemporalScenarioProcessPresenter::elementSelected);

    connect(constraint_presenter,	&TemporalConstraintPresenter::askUpdate,
			this,					&TemporalScenarioProcessPresenter::on_askUpdate);
}

#include "TemporalScenarioProcessPresenter.hpp"

#include "source/Process/ScenarioProcessSharedModel.hpp"
#include "source/Process/Temporal/TemporalScenarioProcessViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioProcessView.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
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
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/RemoveMultipleElements.hpp"

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

	/////// Connections
	connect(this,	SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));

	connect(m_view, &TemporalScenarioProcessView::deletePressed,
			this,	&TemporalScenarioProcessPresenter::on_deletePressed);
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

int TemporalScenarioProcessPresenter::modelId() const
{
	return (SettableIdentifier::identifier_type) m_viewModel->sharedProcessModel()->id();
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

void TemporalScenarioProcessPresenter::on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintViewModelId)
{
	vec_erase_remove_if(m_constraints,
						[&constraintViewModelId] (TemporalConstraintPresenter* pres)
						{
							bool to_delete = pres->viewModel()->id() == constraintViewModelId;
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
	m_view->update();
}


void TemporalScenarioProcessPresenter::on_constraintMoved(id_type<ConstraintModel> constraintId)
{
	auto rect = m_view->boundingRect();

	for(TemporalConstraintPresenter* cstr_pres : m_constraints)
	{
		ConstraintModel* cstr_model{cstr_pres->viewModel()->model()};
		if(cstr_model->id() == constraintId )
		{
			cstr_pres->view()->setPos({qreal(cstr_model->startDate()) / m_millisecPerPixel,
									   rect.height() * cstr_model->heightPercentage()});

			cstr_pres->view()->setWidth(cstr_model->defaultDuration() / m_millisecPerPixel);
		}
	}
	m_view->update();
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS

void TemporalScenarioProcessPresenter::on_deletePressed()
{
	deleteSelection();
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


void TemporalScenarioProcessPresenter::on_scenarioPressedWithControl(QPointF point)
{
	// @todo maybe better to create event on mouserelease ? And only show a "fake" event + interval on mousepress.
	auto cmd = new Command::CreateEvent(ObjectPath::pathFromObject("BaseConstraintModel",
																 m_viewModel->sharedProcessModel()),
									 point.x() * m_millisecPerPixel,
									 (point - m_view->boundingRect().topLeft() ).y() / m_view->boundingRect().height() );
	this->submitCommand(cmd);
}

void TemporalScenarioProcessPresenter::on_scenarioReleased(QPointF point, QPointF scenePoint)
{
	if (point.x() - (m_events.back()->model()->date() / m_millisecPerPixel) > 20 ) // @todo use a const to do that !
	{
		EventData data{};
		data.eventClickedId = m_events.back()->id();
		data.x = point.x();
		data.dDate = point.x() * m_millisecPerPixel;
		data.y = point.y();
		data.scenePos = scenePoint;
		createConstraint(data);
	}
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

void TemporalScenarioProcessPresenter::deleteSelection()
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
												   constraint->viewModel()->model())));
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
	id_type<EventModel> endEvent{0};

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
	// @todo : use relative t and not absolute, so we can move constraint on vertical axis without unfortunately change t position.
	data.dDate = data.x * m_millisecPerPixel;
	data.relativeY = data.y / m_view->boundingRect().height();

	auto cmd = new Command::MoveConstraint(ObjectPath::pathFromObject("BaseConstraintModel",
																	m_viewModel->sharedProcessModel()),
									data);

	submitCommand(cmd);
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


//	connect(event_presenter, &EventPresenter::linesExtremityChange,

	/*[event_view, this] (double top, double bottom)
			{
				event_view->setLinesExtremity(top * m_view->boundingRect().height(),
											  bottom * m_view->boundingRect().height());
			}); */
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

	timeNode_view->setExtremities(-30, 30);

	m_timeNodes.push_back(timeNode_presenter);
}


void TemporalScenarioProcessPresenter::on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model)
{
	auto rect = m_view->boundingRect();

	auto constraint_view = new TemporalConstraintView{constraint_view_model, m_view};
	auto constraint_presenter = new TemporalConstraintPresenter{
													constraint_view_model,
													constraint_view,
													this};

	constraint_view->setWidth(constraint_view_model->model()->defaultDuration() / m_millisecPerPixel);
	constraint_view->setPos({rect.x() + constraint_view_model->model()->startDate() / m_millisecPerPixel,
							 rect.y() + rect.height() * constraint_view_model->model()->heightPercentage()});

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

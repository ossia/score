#include "EventModel.hpp"

#include "Document/Event/State/State.hpp"

#include <API/Headers/Editor/TimeNode.h>

#include <QVector>


EventModel::EventModel(id_type<EventModel> id, QObject* parent):
	IdentifiedObject<EventModel>{id, "EventModel", parent},
	m_timeEvent{new OSSIA::TimeNode}
{
}

EventModel::EventModel(id_type<EventModel> id, double yPos, QObject *parent):
	EventModel{id, parent}
{
    m_heightPercentage = yPos;
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

EventModel::EventModel(EventModel* source,
					   id_type<EventModel> id,
					   QObject* parent):
	EventModel{id, parent}
{
	m_timeNode = source->timeNode();
	m_previousConstraints = source->previousConstraints();
	m_nextConstraints = source->nextConstraints();
	m_constraintsYPos = source->constraintsYPos();
	m_heightPercentage = source->heightPercentage();
	m_topY = source->topY();
	m_bottomY = source->bottomY();

	for(State* state : source->m_states)
	{
		FakeState* newstate = new FakeState{state->id(), this};
		newstate->addMessage(state->messages().first());
		addState(newstate);
	}

	m_condition = source->condition();
	m_date = source->date();
}

EventModel::~EventModel()
{
	delete m_timeEvent;
}

const QVector<id_type<ConstraintModel>>& EventModel::previousConstraints() const
{
	return m_previousConstraints;
}

const QVector<id_type<ConstraintModel> >& EventModel::nextConstraints() const
{
	return m_nextConstraints;
}

void EventModel::addNextConstraint(id_type<ConstraintModel> constraint)
{
	m_nextConstraints.push_back(constraint);
}

void EventModel::addPreviousConstraint(id_type<ConstraintModel> constraint)
{
	m_previousConstraints.push_back(constraint);
}

// TODO refactor this with a small template
bool EventModel::removeNextConstraint(id_type<ConstraintModel> constraintToDelete)
{
	if (m_nextConstraints.indexOf(constraintToDelete) >= 0)
	{
		m_nextConstraints.remove(nextConstraints().indexOf(constraintToDelete));
		m_constraintsYPos.remove(constraintToDelete);
		return true;
	}
	return false;
}

bool EventModel::removePreviousConstraint(id_type<ConstraintModel> constraintToDelete)
{
	if (m_previousConstraints.indexOf(constraintToDelete) >= 0)
	{
		m_previousConstraints.remove(m_previousConstraints.indexOf(constraintToDelete));
		m_constraintsYPos.remove(constraintToDelete);
		return true;
	}
	return false;
}

void EventModel::changeTimeNode(id_type<TimeNodeModel> newTimeNodeId)
{
	m_timeNode = newTimeNodeId;
}

id_type<TimeNodeModel> EventModel::timeNode() const
{
	return m_timeNode;
}

double EventModel::heightPercentage() const
{
	return m_heightPercentage;
}

TimeValue EventModel::date() const
{
	return m_date;
}

void EventModel::setDate(TimeValue date)
{
	m_date = date;
} //TODO ajuster la date avec celle du Timenode

void EventModel::setTopY(double val)
{
	if(val < 0)
	{
		val = 0;
	}
	m_topY = val;
}

void EventModel::setBottomY(double val)
{
	if (val > 1)
	{
		val = 1.0;
	}
	m_bottomY = val;
}

void EventModel::translate(TimeValue deltaTime)
{
	m_date = m_date + deltaTime;
}

void EventModel::setVerticalExtremity(id_type<ConstraintModel> consId, double newPosition)
{
	m_constraintsYPos[consId] = newPosition;
}

// Maybe remove the need for this by passing to the scenario instead ?
#include "Process/ScenarioProcessSharedModel.hpp"
ScenarioProcessSharedModel* EventModel::parentScenario() const
{
	return dynamic_cast<ScenarioProcessSharedModel*>(parent());
}

QString EventModel::condition() const
{
    return m_condition;
}

const std::vector<State*>&EventModel::states() const
{
	return m_states;
}

void EventModel::addState(State* state)
{
	m_states.push_back(state);
	emit messagesChanged();
}

void EventModel::removeState(id_type<State> stateId)
{
	removeById(m_states, stateId);
	emit messagesChanged();
}

void EventModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}


void EventModel::setCondition(QString arg)
{
	if (m_condition == arg)
		return;

	m_condition = arg;
	emit conditionChanged(arg);
}

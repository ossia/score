#include "EventModel.hpp"

#include "Document/Event/State/State.hpp"

#include <API/Headers/Editor/TimeNode.h>

#include <QVector>


QDataStream& operator << (QDataStream& s, const EventModel& ev)
{
	s << ev.m_previousConstraints
	  << ev.m_nextConstraints
	  << ev.m_heightPercentage
	  << ev.m_constraintsYPos
	  << ev.m_bottomY
	  << ev.m_topY;

	s << ev.m_x; // should be in OSSIA API

	s << int(ev.m_states.size());
	for(auto& state : ev.m_states)
	{
		s << *state;
	}

	// TODO s << ev.m_timeNode->save();
	return s;
}

QDataStream& operator >> (QDataStream& s, EventModel& ev)
{
	s >> ev.m_previousConstraints
	  >> ev.m_nextConstraints
	  >> ev.m_heightPercentage
	  >> ev.m_constraintsYPos
	  >> ev.m_bottomY
	  >> ev.m_topY;

	s >> ev.m_x; // should be in OSSIA API

	int numStates{};
	s >> numStates;
	for(; numStates --> 0;)
	{
		ev.createState(s);
	}


	ev.m_timeNode = new OSSIA::TimeNode;
	// TODO load the timenode

	return s;
}


// TODO possibilité temporaire pour tester ce que l'on veut faire :
// FAire une copie du scénario, appliquer la commande dessus, et si elle throw pas sur la copie, l'appliquer pour de vrai.

EventModel::EventModel(int id, QObject* parent):
	IdentifiedObject{id, "EventModel", parent},
	m_timeNode{new OSSIA::TimeNode}
{
	// TODO : connect to the timenode handlers so that the links to the constraints are correctly created.
}

EventModel::EventModel(int id, double yPos, QObject *parent):
	EventModel(id,parent)
{
	m_heightPercentage = yPos;
}


EventModel::EventModel(QDataStream& s, QObject* parent):
	IdentifiedObject{s, "EventModel", parent}
{
	s >> *this;
}

const QVector<int>&EventModel::previousConstraints() const
{
	return m_previousConstraints;
}

const QVector<int>&EventModel::nextConstraints() const
{
	return m_nextConstraints;
}

void EventModel::addNextConstraint(int constraint)
{
	m_nextConstraints.push_back(constraint);
}

void EventModel::addPreviousConstraint(int constraint)
{
	m_previousConstraints.push_back(constraint);
}

bool EventModel::removeNextConstraint(int constraintToDelete)
{
	if (m_nextConstraints.indexOf(constraintToDelete) >= 0)
	{
		m_nextConstraints.remove(nextConstraints().indexOf(constraintToDelete));
		m_constraintsYPos.remove(constraintToDelete);
		return true;
	}
	return false;
}

bool EventModel::removePreviousConstraint(int constraintToDelete)
{
	if (m_previousConstraints.indexOf(constraintToDelete) >= 0)
	{
		m_previousConstraints.remove(m_previousConstraints.indexOf(constraintToDelete));
		m_constraintsYPos.remove(constraintToDelete);
		return true;
	}
	return false;
}

double EventModel::heightPercentage() const
{
	return m_heightPercentage;
}

int EventModel::date() const
{
	return m_x;
}

void EventModel::setDate(int date)
{
	m_x = date;
}

void EventModel::translate(int deltaTime)
{
	m_x += deltaTime;
}

void EventModel::setVerticalExtremity(int consId, double newPosition)
{
	m_constraintsYPos[consId] = newPosition;
	updateVerticalLink();
}

void EventModel::updateVerticalLink()
{
	m_topY = m_heightPercentage;
	m_bottomY = m_heightPercentage;

	for (auto pos : m_constraintsYPos)
	{
		if (pos < m_topY)
		{
			m_topY = pos;
		}
		else if (pos > m_bottomY)
		{
			m_bottomY = pos;
		}
	}
	emit verticalExtremityChanged(m_topY, m_bottomY);
}

// Maybe remove the need for this by passing to the scenario instead ?
#include "Process/ScenarioProcessSharedModel.hpp"
ScenarioProcessSharedModel* EventModel::parentScenario() const
{
	return dynamic_cast<ScenarioProcessSharedModel*>(parent());
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

void EventModel::removeState(int stateId)
{
	removeById(m_states, stateId);
	emit messagesChanged();
}

void EventModel::createState(QDataStream& s)
{
	FakeState* state = new FakeState{s, this};
	m_states.push_back(state);
	emit messagesChanged();
}

void EventModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}

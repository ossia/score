#include "EventModel.hpp"

#include "Document/Event/State/State.hpp"

#include <API/Headers/Editor/TimeNode.h>

#include <QVector>


QDataStream& operator << (QDataStream& s, const EventModel& ev)
{
	s << ev.m_previousConstraints
	  << ev.m_nextConstraints
	  << ev.m_heightPercentage;

	// TODO s << ev.m_timeNode->save();
	return s;
}

QDataStream& operator >> (QDataStream& s, EventModel& ev)
{
	s >> ev.m_previousConstraints
	  >> ev.m_nextConstraints
	  >> ev.m_heightPercentage;

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
	m_states.push_back(new FakeState{this});
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
        return true;
    }
    return false;
}

bool EventModel::removePreviousConstraint(int constraintToDelete)
{
    if (m_previousConstraints.indexOf(constraintToDelete) >= 0)
    {
        m_previousConstraints.remove(m_previousConstraints.indexOf(constraintToDelete));
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

void EventModel::setVerticalExtremity(double newPosition)
{
    /*
    if(newPosition < m_topY)
    {
        m_topY = newPosition;
    }
    else
    {
        m_bottomY = newPosition;
    }
*/
    m_topY = newPosition;
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

void EventModel::addMessage(QString s)
{
	FakeState* state = new FakeState{this};
	state->addMessage(s);

	m_states.push_back(state);

	emit messagesChanged();
}

void EventModel::removeMessage(QString s)
{
	auto state_it = std::find_if(std::begin(m_states),
								 std::end(m_states),
								 [&] (State* state) { return state->messages()[0] == s; });

	if(state_it != std::end(m_states))
	{
		delete *state_it;
		m_states.erase(state_it);
	}

	emit messagesChanged();
}

void EventModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}

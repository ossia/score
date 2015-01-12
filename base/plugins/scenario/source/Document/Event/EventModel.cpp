#include "EventModel.hpp"

#include "Document/Event/State/State.hpp"

#include <API/Headers/Editor/TimeNode.h>

#include <QVector>


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
        updateVerticalLink();
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
        updateVerticalLink();
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
	return m_date;
}

void EventModel::setDate(int date)
{
    m_date = date;
}

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

void EventModel::translate(int deltaTime)
{
	m_date += deltaTime;
}

void EventModel::setVerticalExtremity(int consId, double newPosition)
{
    m_constraintsYPos[consId] = newPosition;
    updateVerticalLink();
}

void EventModel::updateVerticalLink()
{
    m_topY = 0.0;
    m_bottomY = 0.0;
	for (auto pos : m_constraintsYPos)
	{
        pos -= m_heightPercentage;
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

void EventModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
        m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
        updateVerticalLink();
	}
}

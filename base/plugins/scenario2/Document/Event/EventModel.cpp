#include "EventModel.hpp"
#include <API/Headers/Editor/TimeNode.h>
#include <QVector>

// TODO possibilité temporaire pour tester ce que l'on veut faire :
// FAire une copie du scénario, appliquer la commande dessus, et si elle throw pas sur la copie, l'appliquer pour de vrai.

EventModel::EventModel(int id, QObject* parent):
	QIdentifiedObject{parent, "EventModel", id},
	m_timeNode{new OSSIA::TimeNode}
{
	// TODO : connect to the timenode handlers so that the links to the intervals are correctly created.

}

EventModel::EventModel(int id, double yPos, QObject *parent):
	EventModel(id,parent)
{
	m_heightPercentage = yPos;
}

QDataStream& operator << (QDataStream& s, const EventModel& ev)
{
	s << ev.id()
	  << ev.m_previousIntervals
	  << ev.m_nextIntervals
	  << ev.m_heightPercentage;

	// TODO s << ev.m_timeNode->save();
	return s;
}

EventModel::EventModel(QDataStream& s, QObject* parent):
	QIdentifiedObject{nullptr, "EventModel", -1}
{
	int id;
	s >> id >> m_previousIntervals >> m_nextIntervals >> m_heightPercentage;
	this->setId(id);

	m_timeNode = new OSSIA::TimeNode;
	// TODO load the timenode
	this->setParent(parent); // Safer, if somebody receives a signal

}

const QVector<int>&EventModel::previousIntervals() const
{
	return m_previousIntervals;
}

const QVector<int>&EventModel::nextIntervals() const
{
	return m_nextIntervals;
}

double EventModel::heightPercentage() const
{
	return m_heightPercentage;
}

void EventModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}

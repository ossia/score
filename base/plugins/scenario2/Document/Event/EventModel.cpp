#include "EventModel.hpp"
#include <API/Headers/Editor/TimeNode.h>

EventModel::EventModel(int id, QObject* parent):
	QIdentifiedObject{parent, "EventModel", id},
	m_timeNode{new OSSIA::TimeNode}
{
	
}

QDataStream& operator << (QDataStream&, const EventModel&)
{
	qDebug(Q_FUNC_INFO);
}

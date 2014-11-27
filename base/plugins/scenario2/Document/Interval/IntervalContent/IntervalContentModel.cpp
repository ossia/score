#include "IntervalContentModel.hpp"
#include "Interval/IntervalModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"
#include <utilsCPP11.hpp>
#include <QDebug>
QDataStream& operator << (QDataStream& s, const IntervalContentModel& c)
{
	qDebug() << Q_FUNC_INFO;
	s << c.id();
	
	s << (int)c.m_storeys.size(); qDebug() << "Storey size : " << c.m_storeys.size();
	for(auto& storey : c.m_storeys)
	{
		s << *storey;
	}
	s << c.m_nextStoreyId;
}

IntervalContentModel::IntervalContentModel(int id, IntervalModel* parent):
	QIdentifiedObject{parent, "IntervalContentModel", id}
{
	
}

int IntervalContentModel::createStorey()
{
	auto storey = new PositionedStoreyModel{(int) m_storeys.size(), 
											m_nextStoreyId++, 
											this};
	connect(this,	&IntervalContentModel::on_deleteSharedProcessModel, 
			storey, &PositionedStoreyModel::on_deleteSharedProcessModel);
	m_storeys.push_back(storey);
	
	emit storeyCreated(storey->id());
	return storey->id();
}

void IntervalContentModel::deleteStorey(int storeyId)
{
	emit storeyDeleted(storeyId);
	
	removeById(m_storeys, storeyId);
	m_nextStoreyId--;
}

void IntervalContentModel::changeStoreyOrder(int storeyId, int position)
{
}

PositionedStoreyModel* IntervalContentModel::storey(int storeyId)
{
	return findById(m_storeys, storeyId);
}

void IntervalContentModel::duplicateStorey()
{
	
}


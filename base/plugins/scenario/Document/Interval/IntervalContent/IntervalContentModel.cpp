#include "IntervalContentModel.hpp"
#include "Interval/IntervalModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"
#include <tools/utilsCPP11.hpp>
#include <QDebug>
QDataStream& operator << (QDataStream& s, const IntervalContentModel& c)
{
	s << (int)c.m_storeys.size();
	for(auto& storey : c.m_storeys)
	{
		s << *storey;
	}

	return s;
}

QDataStream& operator >> (QDataStream& s, IntervalContentModel& c)
{
	int storeys_size;
	s >> storeys_size;
	for(; storeys_size --> 0 ;)
	{
		c.createStorey(s);
	}

	return s;
}


IntervalContentModel::IntervalContentModel(int id, IntervalModel* parent):
	IdentifiedObject{id, "IntervalContentModel", parent}
{

}

IntervalContentModel::IntervalContentModel(QDataStream& s, IntervalModel* parent):
	IdentifiedObject{s, "IntervalContentModel", parent}
{
	s >> *this;
}

int IntervalContentModel::createStorey()
{
	return createStorey_impl(
				new PositionedStoreyModel{(int) m_storeys.size(),
										  getNextId(m_storeys),
										  this});

}

int IntervalContentModel::createStorey(QDataStream& s)
{
	return createStorey_impl(
				new PositionedStoreyModel{s,
										  this});
}

int IntervalContentModel::createStorey_impl(PositionedStoreyModel* storey)
{
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
}

void IntervalContentModel::changeStoreyOrder(int storeyId, int position)
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

PositionedStoreyModel* IntervalContentModel::storey(int storeyId) const
{
	return findById(m_storeys, storeyId);
}

void IntervalContentModel::duplicateStorey()
{
	qDebug() << Q_FUNC_INFO << "TODO";
}



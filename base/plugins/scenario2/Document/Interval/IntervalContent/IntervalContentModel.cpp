#include "IntervalContentModel.hpp"
#include "Interval/IntervalModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"
#include <utilsCPP11.hpp>
#include <QDebug>
QDataStream& operator << (QDataStream& s, const IntervalContentModel& c)
{
	s << c.id();

	s << (int)c.m_storeys.size();
	for(auto& storey : c.m_storeys)
	{
		s << *storey;
	}
}

IntervalContentModel::IntervalContentModel(int id, IntervalModel* parent):
	QIdentifiedObject{parent, "IntervalContentModel", id}
{

}

IntervalContentModel::IntervalContentModel(QDataStream& s, IntervalModel* parent):
	QIdentifiedObject{nullptr, "IntervalContentModel", -1}
{
	qDebug(Q_FUNC_INFO);
	int id;
	s >> id;
	this->setId(id);
	this->setParent(parent);

	int storeys_size;
	s >> storeys_size;
	for(int i = 0; i < storeys_size; i++)
	{
		createStorey(s);
	}


}

// TODO refactor stuff like this
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

int IntervalContentModel::createStorey(QDataStream& s)
{
	auto storey = new PositionedStoreyModel{s, this};
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


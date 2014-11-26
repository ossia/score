#include "IntervalContentModel.hpp"
#include "Interval/IntervalModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"
#include <utilsCPP11.hpp>
IntervalContentModel::IntervalContentModel(int id, IntervalModel* parent):
	QIdentifiedObject{parent, "IntervalContentModel", id}
{
	
}

void IntervalContentModel::createStorey()
{
	auto storey = new PositionedStoreyModel{(int) m_storeys.size(), 
											m_nextStoreyId++, 
											this};
	m_storeys.push_back(storey);
	
	emit storeyCreated(storey->id());
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

#include "IntervalContentModel.hpp"
#include "Interval/IntervalModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"
#include <utilsCPP11.hpp>
IntervalContentModel::IntervalContentModel(IntervalModel* parent):
	QNamedObject{parent, "IntervalContentModel"}
{
	
}

void IntervalContentModel::createStorey()
{
	auto storey = new PositionedStoreyModel{m_storeys.size(), 
											m_nextStoreyId++, 
											this};
	m_storeys.push_back(storey);
	
	emit storeyCreated(storey->id());
}

void IntervalContentModel::deleteStorey(int storeyId)
{
	emit storeyDeleted(storeyId);
	
	vec_erase_remove_if(m_storeys, 
					   [&storeyId] (PositionedStoreyModel* model) 
						  { 
							  bool to_delete = model->id() == storeyId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}

void IntervalContentModel::changeStoreyOrder(int storeyId, int position)
{
}

void IntervalContentModel::duplicateStorey()
{
	
}

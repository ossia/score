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

PositionedStoreyModel* IntervalContentModel::storey(int storeyId)
{
	auto it = std::find_if(std::begin(m_storeys),
						   std::end(m_storeys),
						   [&storeyId] (StoreyModel* model)
							{
							  return model->id() == storeyId;
							});
	
	if(it != std::end(m_storeys))
		return *it;
	
	return nullptr;
}

void IntervalContentModel::duplicateStorey()
{
	
}

#include "IntervalContentPresenter.hpp"
#include "IntervalContentModel.hpp"

#include "Storey/StoreyPresenter.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"

IntervalContentPresenter::IntervalContentPresenter(IntervalContentModel* model, QObject* parent):
	QNamedObject{parent, "IntervalContentPresenter"},
	m_model{model}
{
	for(auto& storey : m_model->storeys())
	{
		m_storeys.push_back(new StoreyPresenter{storey, this});
	}
}

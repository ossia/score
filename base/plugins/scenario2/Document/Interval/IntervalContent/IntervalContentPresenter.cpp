#include "IntervalContentPresenter.hpp"
#include "IntervalContentModel.hpp"
#include "IntervalContentView.hpp"

#include "Storey/StoreyPresenter.hpp"
#include "Storey/StoreyView.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"

IntervalContentPresenter::IntervalContentPresenter(IntervalContentModel* model,
												   IntervalContentView* view,
												   QObject* parent):
	QNamedObject{parent, "IntervalContentPresenter"},
	m_model{model},
	m_view{view}
{
	for(auto& storeyModel : m_model->storeys())
	{
		auto storeyView = new StoreyView{view};
		m_storeys.push_back(new StoreyPresenter{storeyModel,
												storeyView,
												this});
	}
}

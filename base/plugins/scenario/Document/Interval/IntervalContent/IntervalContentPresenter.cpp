#include "IntervalContentPresenter.hpp"
#include "IntervalContentModel.hpp"
#include "IntervalContentView.hpp"

#include "Storey/StoreyPresenter.hpp"
#include "Storey/StoreyView.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <QGraphicsScene>
#include <tools/utilsCPP11.hpp>

IntervalContentPresenter::IntervalContentPresenter(IntervalContentModel* model,
												   IntervalContentView* view,
												   QObject* parent):
	NamedObject{parent, "IntervalContentPresenter"},
	m_model{model},
	m_view{view}
{
	for(auto& storeyModel : m_model->storeys())
	{
		on_storeyCreated_impl(storeyModel);
	}

	connect(m_model,	&IntervalContentModel::storeyCreated,
			this,		&IntervalContentPresenter::on_storeyCreated);
	connect(m_model,	&IntervalContentModel::storeyDeleted,
			this,		&IntervalContentPresenter::on_storeyDeleted);
}

IntervalContentPresenter::~IntervalContentPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

void IntervalContentPresenter::on_storeyCreated(int storeyId)
{
	on_storeyCreated_impl(m_model->storey(storeyId));
}

void IntervalContentPresenter::on_storeyCreated_impl(StoreyModel* storeyModel)
{
	auto storeyView = new StoreyView{m_view};
	auto storeyPres = new StoreyPresenter{storeyModel,
					  storeyView,
					  this};
	m_storeys.push_back(storeyPres);


	connect(storeyPres, &StoreyPresenter::submitCommand,
			this,		&IntervalContentPresenter::submitCommand);
	connect(storeyPres, &StoreyPresenter::elementSelected,
			this,		&IntervalContentPresenter::elementSelected);
}

void IntervalContentPresenter::on_storeyDeleted(int storeyId)
{
	removeFromVectorWithId(m_storeys, storeyId);
	m_view->update();
}

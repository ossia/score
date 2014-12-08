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
	QNamedObject{parent, "IntervalContentPresenter"},
	m_model{model},
	m_view{view}
{
	for(auto& storeyModel : m_model->storeys())
	{
		on_storeyCreated_impl(storeyModel);
	}

	connect(this, SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));

	connect(this, SIGNAL(elementSelected(QObject*)),
			parent, SIGNAL(elementSelected(QObject*)));

	connect(m_model, SIGNAL(storeyCreated(int)),
			this,	SLOT(on_storeyCreated(int)));
	connect(m_model, SIGNAL(storeyDeleted(int)),
			this,	SLOT(on_storeyDeleted(int)));

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
	m_storeys.push_back(new StoreyPresenter{storeyModel,
											storeyView,
											this});
}

void IntervalContentPresenter::on_storeyDeleted(int storeyId)
{
	removeFromVectorWithId(m_storeys, storeyId);
	m_view->update();
}

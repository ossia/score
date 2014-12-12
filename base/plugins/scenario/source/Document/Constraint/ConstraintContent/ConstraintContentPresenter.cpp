#include "ConstraintContentPresenter.hpp"

#include "Document/Constraint/ConstraintContent/ConstraintContentModel.hpp"
#include "Document/Constraint/ConstraintContent/ConstraintContentView.hpp"
#include "Document/Constraint/ConstraintContent/Storey/StoreyPresenter.hpp"
#include "Document/Constraint/ConstraintContent/Storey/StoreyView.hpp"
#include "Document/Constraint/ConstraintContent/Storey/PositionedStorey/PositionedStoreyModel.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <QGraphicsScene>

ConstraintContentPresenter::ConstraintContentPresenter(ConstraintContentModel* model,
												   ConstraintContentView* view,
												   QObject* parent):
	NamedObject{"ConstraintContentPresenter", parent},
	m_model{model},
	m_view{view}
{
	for(auto& storeyModel : m_model->storeys())
	{
		on_storeyCreated_impl(storeyModel);
	}

	connect(m_model,	&ConstraintContentModel::storeyCreated,
			this,		&ConstraintContentPresenter::on_storeyCreated);
	connect(m_model,	&ConstraintContentModel::storeyDeleted,
			this,		&ConstraintContentPresenter::on_storeyDeleted);
}

ConstraintContentPresenter::~ConstraintContentPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

void ConstraintContentPresenter::on_storeyCreated(int storeyId)
{
	on_storeyCreated_impl(m_model->storey(storeyId));
}

void ConstraintContentPresenter::on_storeyCreated_impl(StoreyModel* storeyModel)
{
	auto storeyView = new StoreyView{m_view};
	auto storeyPres = new StoreyPresenter{storeyModel,
					  storeyView,
					  this};
	m_storeys.push_back(storeyPres);


	connect(storeyPres, &StoreyPresenter::submitCommand,
			this,		&ConstraintContentPresenter::submitCommand);
	connect(storeyPres, &StoreyPresenter::elementSelected,
			this,		&ConstraintContentPresenter::elementSelected);
}

void ConstraintContentPresenter::on_storeyDeleted(int storeyId)
{
	removeFromVectorWithId(m_storeys, storeyId);
	m_view->update();
}

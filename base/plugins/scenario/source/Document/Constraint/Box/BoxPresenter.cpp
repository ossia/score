#include "BoxPresenter.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Storey/StoreyPresenter.hpp"
#include "Document/Constraint/Box/Storey/StoreyView.hpp"
#include "Document/Constraint/Box/Storey/PositionedStorey/PositionedStoreyModel.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/utilsCPP11.hpp>

#include <QGraphicsScene>

BoxPresenter::BoxPresenter(BoxModel* model,
												   BoxView* view,
												   QObject* parent):
	NamedObject{"BoxPresenter", parent},
	m_model{model},
	m_view{view}
{
	for(auto& storeyModel : m_model->storeys())
	{
		on_storeyCreated_impl(storeyModel);
	}

	connect(m_model,	&BoxModel::storeyCreated,
			this,		&BoxPresenter::on_storeyCreated);
	connect(m_model,	&BoxModel::storeyDeleted,
			this,		&BoxPresenter::on_storeyDeleted);
}

BoxPresenter::~BoxPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

void BoxPresenter::on_storeyCreated(int storeyId)
{
	on_storeyCreated_impl(m_model->storey(storeyId));
}

void BoxPresenter::on_storeyCreated_impl(StoreyModel* storeyModel)
{
	auto storeyView = new StoreyView{m_view};
	auto storeyPres = new StoreyPresenter{storeyModel,
					  storeyView,
					  this};
	m_storeys.push_back(storeyPres);


	connect(storeyPres, &StoreyPresenter::submitCommand,
			this,		&BoxPresenter::submitCommand);
	connect(storeyPres, &StoreyPresenter::elementSelected,
			this,		&BoxPresenter::elementSelected);
}

void BoxPresenter::on_storeyDeleted(int storeyId)
{
	removeFromVectorWithId(m_storeys, storeyId);
	m_view->update();
}

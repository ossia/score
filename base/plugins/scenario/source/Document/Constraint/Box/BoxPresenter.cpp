#include "BoxPresenter.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/Storey/StoreyPresenter.hpp"
#include "Document/Constraint/Box/Storey/StoreyView.hpp"
#include "Document/Constraint/Box/Storey/StoreyModel.hpp"

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

	on_askUpdate();

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

int BoxPresenter::height() const
{
	int totalHeight = 25; // No storey -> not visible ? or just "add a process" button ? Bottom bar ? How to make it visible ?
	for(auto& storey : m_storeys)
	{
		totalHeight += storey->height();
	}
	return totalHeight;
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

	connect(storeyPres, &StoreyPresenter::askUpdate,
			this,		&BoxPresenter::on_askUpdate);

	on_askUpdate();
}

void BoxPresenter::updateShape()
{
	m_view->setHeight(height());
	for(StoreyPresenter* storey : m_storeys)
	{
		// set the vertical position of each in the box.
		//storey
	}
}

void BoxPresenter::on_storeyDeleted(int storeyId)
{
	removeFromVectorWithId(m_storeys, storeyId);
	emit askUpdate();
}

void BoxPresenter::on_askUpdate()
{
	updateShape();
	emit askUpdate();
}

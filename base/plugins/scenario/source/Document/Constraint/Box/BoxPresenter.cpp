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
	qDebug() << Q_FUNC_INFO << m_model->storeys().size();
	for(auto& storeyModel : m_model->storeys())
	{
		on_storeyCreated_impl(storeyModel);
	}

	on_askUpdate();

	connect(m_model,	&BoxModel::storeyCreated,
			this,		&BoxPresenter::on_storeyCreated);
	connect(m_model,	&BoxModel::storeyDeleted,
			this,		&BoxPresenter::on_storeyRemoved);
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
	for(auto& storey : m_decks)
	{
		totalHeight += storey->height();
	}
	return totalHeight;
}

int BoxPresenter::id() const
{
	// TODO dangerous : what happens if the model gets removed before ?
	return m_model->id();
}

void BoxPresenter::on_storeyCreated(int storeyId)
{
	on_storeyCreated_impl(m_model->deck(storeyId));
}

void BoxPresenter::on_storeyCreated_impl(StoreyModel* storeyModel)
{
	qDebug() << Q_FUNC_INFO;
	auto storeyView = new StoreyView{m_view};
	storeyView->setPos(5, 5);
	auto storeyPres = new StoreyPresenter{storeyModel,
					  storeyView,
					  this};
	m_decks.push_back(storeyPres);


	connect(storeyPres, &StoreyPresenter::submitCommand,
			this,		&BoxPresenter::submitCommand);
	connect(storeyPres, &StoreyPresenter::elementSelected,
			this,		&BoxPresenter::elementSelected);

	connect(storeyPres, &StoreyPresenter::askUpdate,
			this,		&BoxPresenter::on_askUpdate);

	on_askUpdate();
}

void BoxPresenter::on_storeyRemoved(int storeyId)
{
	removeFromVectorWithId(m_decks, storeyId);
	on_askUpdate();
}

void BoxPresenter::updateShape()
{
	using namespace std;
	m_view->setHeight(height());

	// 1. Make an array with the Decks stored by their positions
	vector<StoreyPresenter*> sortedDecks(m_decks.size());
	for(StoreyPresenter* deck : m_decks)
	{
		sortedDecks[deck->position()] = deck;
	}

	// 2. Set their position in order.
	int currentDeckY = 20;
	for(auto& deck : sortedDecks)
	{
		deck->setVerticalPosition(currentDeckY);
		currentDeckY += deck->height() + 5;
	}
}

void BoxPresenter::on_askUpdate()
{
	updateShape();
	emit askUpdate();
}
